/*
 * Copyright (c) 2015, 2016, 2017, 2018 Rafael Antoniello
 *
 * This file is part of StreamProcessors.
 *
 * StreamProcessors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StreamProcessors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with StreamProcessors.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file ts_dec.c
 * @Author Rafael Antoniello
 */

#include "ts_dec.h"

#include <stdlib.h>
#include <string.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocsutils/bitparser.h>
#include "ts.h"

/* **** Definitions **** */

#define GET_BITS(b) bitparser_get(bitparser_ctx, (b))
#define FLUSH_BITS(b) bitparser_flush(bitparser_ctx, (b))
#define COPY_BYTES(B) bitparser_copy_bytes(bitparser_ctx, (B))

/* **** Prototypes **** */

static int ts_dec_adaptation_field(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint8_t contains_payload, uint16_t pid,
		ts_af_ctx_t **ref_ts_af_ctx);

/* **** Implementations **** */

int ts_dec_get_next_packet(fifo_ctx_t *ififo_ctx, log_ctx_t *log_ctx,
		uint8_t *ref_cc, ts_ctx_t **ref_ts_ctx)
{
	ts_ctx_t *ts_ctx;
	uint16_t pid;
	int ret_code, end_code= STAT_ERROR;
	uint8_t curr_cc= TS_CC_UNDEF, prev_cc= TS_CC_UNDEF;
	uint8_t *pkt= NULL;
	size_t pkt_size= 0;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments.
	 * Arguments 'log_ctx' and 'ref_cc' are allowed to be NULL.
	 */
	CHECK_DO(ififo_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(ref_ts_ctx!= NULL, return STAT_ERROR);

	*ref_ts_ctx= NULL;

	/* Get (flush) next TS packet byte buffer */
	ret_code= fifo_get(ififo_ctx, (void**)&pkt, &pkt_size);
	if(ret_code!= STAT_SUCCESS) {
		if(ret_code== STAT_EAGAIN)
			end_code= STAT_EOF; // FIFO unblocked; we are requested to exit.
		goto end;
	}
	CHECK_DO(pkt!= NULL && pkt[0]== 0x47 && pkt_size== TS_PKT_SIZE, goto end);

	/* Decode TS packet buffer into context structure */
	ret_code= ts_dec_packet(pkt, LOG_CTX_GET(), ref_ts_ctx);
	CHECK_DO(ret_code== STAT_SUCCESS && (ts_ctx= *ref_ts_ctx)!= NULL, goto end);

	/* Update continuity counter registers */
	/* Note that in the case of a null packet, the value of the
	 * continuity_counter is undefined (do not update).
	 */
	pid= ts_ctx->pid;
	if(pid!= 0x1FFF) {
		curr_cc= ts_ctx->continuity_counter;
		ASSERT(curr_cc<= 0x0F);
	}
	if(ref_cc!= NULL) {
		prev_cc= *ref_cc;
		if(curr_cc!= TS_CC_UNDEF)
			*ref_cc= curr_cc; // Update continuity counter external register
	}

	/* **** Check compliance: Continuity counter. **** */
	/* Note that this function ('ts_dec_get_next_packet()') parses packets
	 * extracted from an input FIFO buffer corresponding to a stream carrying a
	 * single packet identifier (PID).
	 * We can not use the next packet of the FIFO to check continuity because
	 * in that case we may be introducing a delay (we should wait for the
	 * next packet to come eventually). In consequence, the valid
	 * solution is to check continuity against the previous packet with the
	 * same PID. The previous packet should be passed by argument by external
	 * means.
	 */

	/* In the case previous or current packet continuity counter are not
	 * defined, we are done.
	 */
	if(prev_cc== TS_CC_UNDEF || curr_cc== TS_CC_UNDEF) {
		end_code= STAT_SUCCESS;
		goto end;
	}

	/* The continuity_counter in a particular Transport Stream packet is
	 * continuous when it differs by a positive value of one from the
	 * continuity_counter value in the previous Transport Stream packet of the
	 * same PID, or when either of the non-incrementing conditions
	 * (adaptation_field_control set to '00' or '10', or duplicate packets)
	 * are met. The continuity counter may be discontinuous when the
	 * discontinuity_indicator is set to '1'.
	 */
	if(prev_cc!= curr_cc) {
		/* Check incrementing conditions */
		/* A continuity error exist if all the following occur:
		 * - A discontinuity exist in counter;
		 * - Discontinuity is not explicitly set.
		 */
		int discontinuity_flag= (((prev_cc+ 1)& 0xF)!= curr_cc);
		int explicit_disc_set_flag= (ts_ctx->adaptation_field_exist &&
				ts_ctx->adaptation_field &&
				ts_ctx->adaptation_field->discontinuity_indicator!= 0);
		if(discontinuity_flag && !explicit_disc_set_flag) {
			/* Continuity error detected */
			LOGE("Continuity error detected at input TS: illegal "
					"incrementing condition (%u to %u). PID= %u (0x%0x).\n",
					prev_cc, curr_cc, pid, pid);
			//goto end; // Do not return error, just report.
		}
		if(ts_ctx->contains_payload== 0) {
			LOGE("Continuity error detected (TS packet without payload does "
					"not met the non-incrementing conditions). "
					"PID= %u (0x%0x).\n", pid, pid);
		}
	} else {
		/* Check if we met the non-incrementing conditions */
		/* Check 'adaptation_field_control' */
		if(ts_ctx->contains_payload!= 0) {
			/* Check if we have a packet duplication or discontinuity error */
			/* In Transport Streams, duplicate packets may be sent as two,
			 * and only two, consecutive Transport Stream packets of the same
			 * PID. The duplicate packets shall have the same
			 * continuity_counter value as the original packet and the
			 * adaptation_field_control field shall be equal to '01' or '11'.
			 * In duplicate packets each byte of the original packet shall be
			 * duplicated, with the exception that in the program clock
			 * reference fields, if present.
			 */
			//TODO: Check duplication at byte level:
			// - If duplicated, continue to
			// next packet ignoring this one, and check strictly incrementing
			// conditions.
			// - Else, report discontinuity.
			LOGE("Continuity error detected (TS packet with payload does "
					"not met the non-incrementing conditions). "
					"PID= %u (0x%0x).\n", pid, pid);
			//goto end;
		}
	}

	end_code= STAT_SUCCESS;
end:
	if(end_code== STAT_ERROR)
		ts_ctx_release(ref_ts_ctx);
	if(pkt!= NULL)
		free(pkt);
	return end_code;
}

static int ts_dec_adaptation_field(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint8_t contains_payload, uint16_t pid,
		ts_af_ctx_t **ref_ts_af_ctx)
{
	uint8_t adaptation_field_length;
	int remaining_bits, end_code= STAT_ERROR;
	ts_af_ctx_t *ts_af_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(bitparser_ctx!= NULL, goto end);
	CHECK_DO(ref_ts_af_ctx!= NULL, goto end);

	*ref_ts_af_ctx= NULL;

	/* Allocate AF */
	ts_af_ctx= ts_af_ctx_allocate();
	CHECK_DO(ts_af_ctx!= NULL, goto end);

	adaptation_field_length= ts_af_ctx->adaptation_field_length= GET_BITS(8);

	/* Check compliance: adaptation field length */
	if((contains_payload && adaptation_field_length> 182) ||
	   (contains_payload== 0 && adaptation_field_length!= 183)) {
		LOGE("Check compliance: Invalid adaptation field length "
				"value: %u. PID= %u (0x%0x).\n", adaptation_field_length, pid,
				pid);
		//goto end; // Avoid discarding; try to continue parsing
	}

	/* Parse adaptation field */
	if(adaptation_field_length> 0) {
		ts_af_ctx->discontinuity_indicator= GET_BITS(1);
		ts_af_ctx->random_access_indicator= GET_BITS(1);
		ts_af_ctx->elementary_stream_priority_indicator= GET_BITS(1);
		ts_af_ctx->pcr_flag= GET_BITS(1);
		ts_af_ctx->opcr_flag= GET_BITS(1);
		ts_af_ctx->splicing_point_flag= GET_BITS(1);
		ts_af_ctx->transport_private_data_flag= GET_BITS(1);
		ts_af_ctx->adaptation_field_extension_flag= GET_BITS(1);
		/* Optional fields */
		if(ts_af_ctx->pcr_flag) {
			ts_af_ctx->pcr = ((uint64_t)GET_BITS(16))<< 32;
			ts_af_ctx->pcr|= GET_BITS(32);
		}
		if(ts_af_ctx->opcr_flag) {
			ts_af_ctx->opcr = ((uint64_t)GET_BITS(16))<< 32;
			ts_af_ctx->opcr|= GET_BITS(32);
		}
		if(ts_af_ctx->splicing_point_flag)
			ts_af_ctx->splice_countdown= GET_BITS(8);

		/* Store rest of stuffing bytes */ //TODO: further parsing
		remaining_bits= (adaptation_field_length *8)- 8-
				(ts_af_ctx->pcr_flag* 48)- (ts_af_ctx->opcr_flag* 48)-
				(ts_af_ctx->splicing_point_flag *8);
		CHECK_DO(remaining_bits>= 0, goto end);

		if(remaining_bits> 0) {
			/* Copy remaining AF bytes to internal buffer */
			ts_af_ctx->af_remaining= (uint8_t*)COPY_BYTES(remaining_bits>> 3);
			ts_af_ctx->af_remaining_size= remaining_bits>> 3;
		}
	}

	*ref_ts_af_ctx= ts_af_ctx;
	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		ts_af_ctx_release(&ts_af_ctx);

	return end_code;
}

int ts_dec_packet(uint8_t *pkt, log_ctx_t *log_ctx, ts_ctx_t **ref_ts_ctx)
{
	uint8_t transport_error_indicator;
	uint8_t payload_unit_start_indicator;
	uint16_t pid;
	uint8_t adaptation_field_exist;
	uint8_t contains_payload;
	int remaining_bytes, ret_code, end_code= STAT_ERROR;
	bitparser_ctx_t *bitparser_ctx= NULL;
	ts_ctx_t* ts_ctx= NULL;
	ts_af_ctx_t *ts_af_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments.
	 * Note: Argument 'log_ctx' is allowed to be 'NULL'.
	 */
	CHECK_DO(pkt!= NULL, return STAT_ERROR);
	CHECK_DO(ref_ts_ctx!= NULL, return STAT_ERROR);

	*ref_ts_ctx= NULL;

	/* Allocate Transport Stream (TS) context structure */
	ts_ctx= ts_ctx_allocate();
	CHECK_DO(ts_ctx!= NULL, goto end);

	/* Initialise bit-parser */
	bitparser_ctx= bitparser_open(pkt, EXTEND_SIZE_TO_MULTIPLE(TS_PKT_SIZE,
			sizeof(WORD_T)));
	CHECK_DO(bitparser_ctx!= NULL, goto end);

	/* Check synchronisation byte (0x47) */
	CHECK_DO(GET_BITS(8)== 0x47, goto end);

	/* **** Parse Transport Stream Header ****
	 * (also so-called "transport stream 4-byte prefix").
	 */
	transport_error_indicator= ts_ctx->transport_error_indicator= GET_BITS(1);
	payload_unit_start_indicator= ts_ctx->payload_unit_start_indicator=
			GET_BITS(1);
	ts_ctx->transport_priority= GET_BITS(1);
	pid= ts_ctx->pid= GET_BITS(13);
	ts_ctx->scrambling_control= GET_BITS(2);
	adaptation_field_exist= ts_ctx->adaptation_field_exist= GET_BITS(1);
	contains_payload= ts_ctx->contains_payload= GET_BITS(1);
	ts_ctx->continuity_counter= GET_BITS(4);

	/* Check compliance: 'transport_error_indicator' */
	if(transport_error_indicator) {
		LOGEV("Transport error indicator specified. PID= %u (0x%0x).\n", pid,
				pid);
		//goto end;
	}

	/* Check compliance: 'payload_unit_start_indicator' (for null packets). */
	if(pid== 0x1FFF) {
		//LOGEV("MPEG2-TS null packet.\n");
		if(payload_unit_start_indicator!= 0) {
			LOGEV("Check compliance: Illegal 'null' MPEG2-TS packet "
					"with 'payload_unit_start_indicator' set.\n");
			goto end;
		}
	}

	/* Check compliance: PID should not take any of the reserved values
	 * described in ISO/IEC 13818-1.
	 */
	if(pid>= 0x0003 && pid<= 0x000F) {
		LOGEV("Check compliance: Illegal PID value %u (PID value is "
				"reserved).\n", pid);
		goto end;
	}

	/* Check compliance: 'scrambling_control'.
	 * If PID is 0x0000, 0x0001 or 0x1FFF, or if the TS Packet contains PMT
	 * sections, 'transport_scrambling_control' should be ‘00’.
	 */
	if(pid== 0 || pid== 1 || pid== 0x1FFF) {
		if(ts_ctx->scrambling_control) {
			LOGEV("Check compliance: Illegal scrambling for PID %u."
					"\n", pid);
			goto end;
		}
	}

	/* Check compliance: 'payload_unit_start_indicator' and
	 * 'adaptation_field_control'.
	 */
	if(payload_unit_start_indicator && !contains_payload) {
		LOGEV("Check compliance: 'payload_unit_start_indicator' set "
				"but without packet payload. PID= %u (0x%0x).\n", pid, pid);
		goto end;
	}

	/* Check compliance: 'adaptation_field_control'.
	 * If the PID is 0x1FFF, 'adaptation_field_control' should be ‘01’, else
	 * it should be one of ‘01’, ‘10’, or ‘11’.
	 */
	if(pid== 0x1FFF) {
		if(adaptation_field_exist || !contains_payload) {
			LOGEV("Check compliance: Illegal 'null' MPEG2-TS packet "
					"with 'adaptation_field_control' not set to '01'. "
					"PID= %u (0x%0x).\n", pid, pid);
			goto end;
		}
	} else {
		if(!adaptation_field_exist && !contains_payload) {
			LOGEV("Check compliance: 'adaptation_field_control' set "
					"to reserved value '00'. PID= %u (0x%0x).\n", pid, pid);
			goto end;
		}
	}

	/* Adaptation field (AF) */
	if(adaptation_field_exist) {
		ret_code= ts_dec_adaptation_field(log_ctx, bitparser_ctx,
				contains_payload, pid, &ts_ctx->adaptation_field);
		if(ret_code!= STAT_SUCCESS) goto end;
		ts_af_ctx= ts_ctx->adaptation_field;
	}

	/* Next bytes should be Payload or stuffing.
	 * Size in bytes: 188- 4-
	 * (1+ adaptation_field_exist* adaptation_field_length).
	 */
	remaining_bytes= TS_PKT_SIZE- TS_PKT_PREFIX_LEN;
	if(ts_af_ctx!= NULL)
		remaining_bytes-= (1+ ts_af_ctx->adaptation_field_length);
	CHECK_DO(remaining_bytes>= 0, goto end);
	//LOGV("Pay-load syze in bytes: %d\n", remaining_bytes); //comment-me

	if(remaining_bytes> 0) {
		if(contains_payload== 0) {
			/* Flush the rest of the bytes if this packet does not contain
			 * payload (e.g.: "dummy packet")
			 */
			FLUSH_BITS(remaining_bytes<< 3);
		} else {
			/* Copy payload to internal buffer */
			ts_ctx->payload= (uint8_t*)COPY_BYTES(remaining_bytes);
			ts_ctx->payload_size= remaining_bytes;
		}
	} else {
		/* This is a "HACK" to permit bypassing TS packets with no payload
		 * bytes but wrong 'adaptation_field_exist' and 'contains_payload'
		 * fields.
		 */
		ts_ctx->payload= NULL;
		ts_ctx->payload_size= 0;
		//LOGV("payload_size: %u\n", ts_ctx->payload_size); //comment-me
		//LOGV("contains_payload: %u\n", ts_ctx->contains_payload); //comment-me
		ts_ctx->contains_payload= 0;
	}

	*ref_ts_ctx= ts_ctx;
	ts_ctx= NULL; // Avoid freeing at the end of function.
	//ts_ctx_trace((const ts_ctx_t*)*ref_ts_ctx); //comment-me
	end_code= STAT_SUCCESS;
end:
	bitparser_close(&bitparser_ctx);
	if(ts_ctx!= NULL)
		ts_ctx_release(&ts_ctx);
	return end_code;
}
