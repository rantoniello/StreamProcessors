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
 * @file ts_enc.c
 * @author Rafael Antoniello
 */

#include "ts_enc.h"

#include <stdlib.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/llist.h>
#include "ts.h"

/* **** Definitions **** */

/* **** Prototypes **** */

/* **** Implementations **** */

int ts_enc_packet(const ts_ctx_t *ts_ctx, log_ctx_t *log_ctx, void **buf,
		size_t *size)
{
	register uint8_t adaptation_field_exist;
	register uint8_t payload_size;
	ts_af_ctx_t *ts_af_ctx= NULL;
	uint8_t opt_field_byte_cnt= 0; // counter
	uint8_t *packet= NULL;
	size_t header_size;
	int end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(ts_ctx!= NULL, goto end);
	CHECK_DO(buf!= NULL, goto end);
	CHECK_DO(size!= NULL, goto end);

	*buf= NULL;
	*size= 0;

	/* Allocate TS packet buffer */
	packet= (uint8_t*)malloc(TS_PKT_SIZE);
	CHECK_DO(packet!= NULL, goto end);

	/* Put the TS context structure in byte buffer */
	packet[0] = 0x47;
	packet[1] = ts_ctx->transport_error_indicator<< 7;
	packet[1]|= ts_ctx->payload_unit_start_indicator<< 6;
	packet[1]|= ts_ctx->transport_priority<< 5;
	packet[1]|= ts_ctx->pid>> 8;
	packet[2] = ts_ctx->pid& 0xFF;
	packet[3] = ts_ctx->scrambling_control<< 6;
	packet[3]|= adaptation_field_exist= ts_ctx->adaptation_field_exist<< 5;
	packet[3]|= ts_ctx->contains_payload<< 4;
	packet[3]|= ts_ctx->continuity_counter;
	header_size= TS_PKT_PREFIX_LEN;
	if(adaptation_field_exist) {
		register uint8_t adaptation_field_length;
		ts_af_ctx= ts_ctx->adaptation_field;
		CHECK_DO(ts_af_ctx!= NULL, goto end);
		packet[4] = adaptation_field_length= ts_af_ctx->adaptation_field_length;
		opt_field_byte_cnt+= 1;
		header_size+= 1+ adaptation_field_length;
		if(adaptation_field_length) {
			register uint8_t pcr_flag;
			register uint8_t opcr_flag;
			register uint8_t splicing_point_flag;
			register uint8_t af_remaining_size;
			packet[5] = ts_af_ctx->discontinuity_indicator<< 7;
			packet[5]|= ts_af_ctx->random_access_indicator<< 6;
			packet[5]|= ts_af_ctx->elementary_stream_priority_indicator<< 5;
			packet[5]|= pcr_flag= ts_af_ctx->pcr_flag<< 4;
			packet[5]|= opcr_flag= ts_af_ctx->opcr_flag<< 3;
			packet[5]|= splicing_point_flag= ts_af_ctx->splicing_point_flag<< 2;
			packet[5]|= ts_af_ctx->transport_private_data_flag<< 1;
			packet[5]|= ts_af_ctx->adaptation_field_extension_flag;
			opt_field_byte_cnt+= 1;
			if(pcr_flag) {
				/* Unless otherwise specified within ITU-T Rec. H.222.0 |
				 * ISO/IEC 13818-1, all reserved bits shall be set to '1'.
				 */
				uint64_t pcr= ts_af_ctx->pcr;
				packet[6 ]= (pcr>> 40)& 0xFF;
				packet[7 ]= (pcr>> 32)& 0xFF;
				packet[8 ]= (pcr>> 24)& 0xFF;
				packet[9 ]= (pcr>> 16)& 0xFF;
				packet[10]= (pcr>>  8)& 0xFF;
				packet[11]= pcr& 0xFF;
				opt_field_byte_cnt+= 6;
			}
			if(opcr_flag) {
				/* Unless otherwise specified within ITU-T Rec. H.222.0 |
				 * ISO/IEC 13818-1, all reserved bits shall be set to '1'.
				 */
				uint64_t opcr= ts_af_ctx->opcr;
				packet[12]= (opcr>> 40)& 0xFF;
				packet[13]= (opcr>> 32)& 0xFF;
				packet[14]= (opcr>> 24)& 0xFF;
				packet[15]= (opcr>> 16)& 0xFF;
				packet[16]= (opcr>>  8)& 0xFF;
				packet[17]= opcr& 0xFF;
				opt_field_byte_cnt+= 6;
			}
			if(splicing_point_flag) {
				packet[18]= ts_af_ctx->splice_countdown;
				opt_field_byte_cnt+= 1;
			}

			/* Copy remaining bytes. */
			af_remaining_size= ts_af_ctx->af_remaining_size;
			CHECK_DO(opt_field_byte_cnt+ af_remaining_size==
					adaptation_field_length+ 1, goto end);
			if(af_remaining_size> 0) {
				CHECK_DO(ts_af_ctx->af_remaining!= NULL, goto end);
				CHECK_DO(TS_PKT_PREFIX_LEN+ opt_field_byte_cnt+
						af_remaining_size<= TS_PKT_SIZE, goto end);
				memcpy(&packet[TS_PKT_PREFIX_LEN+ opt_field_byte_cnt],
						ts_af_ctx->af_remaining, af_remaining_size);
			}
		}
	} // "adaptation field"
	/* Copy payload. */
	payload_size= ts_ctx->payload_size;
	if(payload_size> 0) {
		CHECK_DO(ts_ctx->payload!= NULL, goto end);
		CHECK_DO(header_size+ payload_size== TS_PKT_SIZE, goto end);
		memcpy(&packet[header_size], ts_ctx->payload, payload_size);
	}

	*buf= (void*)packet;
	packet= NULL; // Avoid freeing at the end of function.
	*size= TS_PKT_SIZE;
	end_code= STAT_SUCCESS;
end:
	if(packet!= NULL)
		free(packet);

	return end_code;
}

int ts_enc_pcr_packet(log_ctx_t *log_ctx, uint16_t pid, uint8_t cc,
		int64_t pcr_base, int64_t pcr_ext, void **buf, size_t *size)
{
	uint8_t adaptation_field_length, af_remaining_size;
	int ret_code, end_code= STAT_ERROR;
	ts_ctx_t *ts_ctx= NULL;
	ts_af_ctx_t *ts_af_ctx= NULL;
	uint8_t *af_remaining= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(pid< 0x1FFF, return STAT_ERROR);
	CHECK_DO(cc<= 0x0F, return STAT_ERROR);
	CHECK_DO(buf!= NULL, return STAT_ERROR);
	CHECK_DO(size!= NULL, return STAT_ERROR);

	*buf= NULL;
	*size= 0;

	/* Allocate TS packet context structure.
	 * Note that packet fields are initialized to zero; thus, only set
	 * necessary fields.
	 */
	ts_ctx= ts_ctx_allocate();
	CHECK_DO(ts_ctx!= NULL, goto end);

	ts_ctx->pid= pid;
	ts_ctx->adaptation_field_exist= 1;
	ts_ctx->contains_payload= 0; // Redundant, but just to remark
	ts_ctx->continuity_counter= cc; // No payload-> non-incrementing

	/* Adaptation field.
	 * Only set necessary AF flags for PCR, rest is stuffing.
	 */
	ts_af_ctx= ts_af_ctx_allocate();
	CHECK_DO(ts_af_ctx!= NULL, goto end);
	ts_ctx->adaptation_field= ts_af_ctx;

	adaptation_field_length=
			TS_PKT_SIZE- (TS_PKT_PREFIX_LEN+ 1); // Only AF present
	ts_af_ctx->adaptation_field_length= adaptation_field_length;
	ts_af_ctx->pcr_flag= 1;
	ts_af_ctx->pcr = (pcr_base& (uint64_t)0x1FFFFFFFF)<< 15;
	ts_af_ctx->pcr|= 0x3F<< 9;
	ts_af_ctx->pcr|= pcr_ext& 0x1FF;

	af_remaining_size= adaptation_field_length- 1- 6;
	ts_af_ctx->af_remaining_size= af_remaining_size;
	af_remaining= (uint8_t*)malloc(af_remaining_size);
	CHECK_DO(af_remaining!= NULL, goto end);
	ts_af_ctx->af_remaining= af_remaining;
	memset(af_remaining, 0xFF, af_remaining_size);

	ret_code= ts_enc_packet(ts_ctx, LOG_CTX_GET(), buf, size);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
	ts_ctx_release(&ts_ctx);
	return end_code;
}

int ts_enc_stuffing_packet(log_ctx_t *log_ctx, uint16_t pid, uint8_t cc,
		void **ref_buf, size_t *ref_size)
{
	int ret_code, end_code= STAT_ERROR;
	ts_ctx_t *ts_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(pid< 0x1FFF, return STAT_ERROR);
	CHECK_DO(cc<= 0x0F, return STAT_ERROR);
	CHECK_DO(ref_buf!= NULL, return STAT_ERROR);
	CHECK_DO(ref_size!= NULL, return STAT_ERROR);

	*ref_buf= NULL;
	*ref_size= 0;

	/* Allocate TS packet context structure.
	 * Note that packet fields are initialised to zero; thus, only set
	 * necessary fields.
	 */
	ts_ctx= ts_ctx_allocate();
	CHECK_DO(ts_ctx!= NULL, goto end);

	/* Just compose a single stuffing PID-packet to be sent */
	//ts_ctx->transport_error_indicator= 0;
	//ts_ctx->payload_unit_start_indicator= 0;
	//ts_ctx->transport_priority= 0;
	ts_ctx->pid= pid;
	//ts_ctx->scrambling_control= 0;
	//ts_ctx->adaptation_field_exist=
	//		0; // Allocated and filled at "" method below
	//ts_ctx->contains_payload= 0; // No pay-load
	ts_ctx->continuity_counter= cc; // No pay-load-> non-incrementing
	//ts_ctx->adaptation_field= NULL;
	//ts_ctx->payload= NULL;
	//ts_ctx->payload_size= 0;
	// Fill with stuffing all the adaptation field
	ret_code= ts_enc_add_adaptation_field_stuffing(ts_ctx,
			TS_PKT_SIZE-TS_PKT_PREFIX_LEN, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	ret_code= ts_enc_packet(ts_ctx, LOG_CTX_GET(), ref_buf, ref_size);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
	if(ts_ctx!= NULL)
		ts_ctx_release(&ts_ctx);
	return end_code;
}

int ts_enc_pcrrestamp(void *buf, int64_t pcr_base, int64_t pcr_ext)
{
	uint8_t *ts_buf;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(buf!= NULL, return STAT_ERROR);

	ts_buf= (uint8_t*)buf;

	/* Re-stamp TS packet if applicable */
	if((ts_buf[3]&0x20) && // Adaptation field flag is set
	   (ts_buf[4]!= 0) && // Adaptation field length > 0
	   (ts_buf[5]& 0x10)) { // PCR flag is set
		/* We have PCR.
		 * Restore as 33 bits base, 6 bits reserved, 9 bits extension.
		 */
		ts_buf[ 6]= ((pcr_base>> 25)& 0xFF);
		ts_buf[ 7]= ((pcr_base>> 17)& 0xFF);
		ts_buf[ 8]= ((pcr_base>>  9)& 0xFF);
		ts_buf[ 9]= ((pcr_base>>  1)& 0xFF);
		ts_buf[10]= ((pcr_base& 1)<< 7)| 0x7E| ((pcr_ext& 0x100)>> 8);
		ts_buf[11]= pcr_ext& 0xFF;
		return STAT_SUCCESS;
	}
	return STAT_NOTMODIFIED;
}

int ts_enc_add_adaptation_field_stuffing(ts_ctx_t *ts_ctx, uint8_t stuff_size,
		log_ctx_t *log_ctx)
{
	ts_af_ctx_t *ts_af_ctx;
	uint8_t adaptation_field_length;
	register int end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(ts_ctx!= NULL, return STAT_ERROR);
	if(stuff_size== 0)
		return STAT_SUCCESS;

	/* **** Add stuffing bytes to adaptation field (AF) **** */

	/* - Create AF if applicable */
	if(ts_ctx->adaptation_field_exist== 0) {
		ts_ctx->adaptation_field_exist= 1;
		CHECK_DO(ts_ctx->adaptation_field== NULL, goto end);
		ts_ctx->adaptation_field= ts_af_ctx_allocate();
		/* - Adding a single stuffing byte can be achieved using only
		 * 'adaptation_field_length= 0' (special case). Thus, if AF was just
		 * allocated above, one of the requested stuffing bytes can be
		 * consumed by the 'adaptation_field_length' byte.
		 * - Note that 'adaptation_field_length' was correctly initialized to
		 * zero in corresponding allocation function.
		 */
		stuff_size-= 1;
	}

	/* Get AF context structure */
	ts_af_ctx= ts_ctx->adaptation_field;
	CHECK_DO(ts_af_ctx!= NULL, goto end);

	adaptation_field_length= ts_af_ctx->adaptation_field_length;

	/* - In the case 'adaptation_field_length' is currently zero, one of the
	 * requested stuffing bytes can be consumed by using the first eight flags
	 * carried in the AF (that is, from 'discontinuity_indicator' field to
	 * 'adaptation_field_extension_flag').
	 */
	if(stuff_size> 0 && adaptation_field_length== 0) {
		adaptation_field_length+= 1;
		ts_af_ctx->adaptation_field_length= adaptation_field_length;
		stuff_size-= 1;
	}

	/* - At this point 'adaptation_field_length' is greater than zero,
	 * thus the rest of requested stuffing bytes are added at the end of the AF
	 */
	if(stuff_size> 0) {
		register void *p;
		register uint8_t *af_remaining;
		register uint8_t af_remaining_size;

		af_remaining= ts_af_ctx->af_remaining;
		af_remaining_size= ts_af_ctx->af_remaining_size;

		/* - Reallocate AF "remaining" bytes pointer */
		p= realloc((void*)af_remaining, af_remaining_size+ stuff_size);
		CHECK_DO(p!= NULL, goto end);
		af_remaining= (uint8_t*)p;
		ts_af_ctx->af_remaining= af_remaining;

		/* Set new stuffing bytes to legal values (0xFF) */
		p= (void*)(af_remaining+ af_remaining_size);
		memset(p, 0xFF, (size_t)stuff_size);

		/* - Update AF "remaining" bytes declared size */
		af_remaining_size+= stuff_size;
		ts_af_ctx->af_remaining_size= af_remaining_size;

		/* - Update AF declared size */
		adaptation_field_length+= stuff_size;
		ts_af_ctx->adaptation_field_length= adaptation_field_length;
	}

	end_code= STAT_SUCCESS;
end:
	return end_code;
}
