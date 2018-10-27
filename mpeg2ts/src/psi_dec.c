/*
 * Copyright (c) 2015 Rafael Antoniello
 *
 * This file is part of StreamProcessor.
 *
 * StreamProcessor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StreamProcessor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with StreamProcessor.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file psi_dec.c
 * @Author Rafael Antoniello
 */

#include "psi_dec.h"

#include <stdlib.h>
#include <string.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/bitparser.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocsutils/llist.h>
#include <libmediaprocsutils/crc_32_mpeg2.h>
#include "psi.h"
#include "psi_dvb.h"
#include "psi_dvb_dec.h"
#include "psi_desc.h"
#include "psi_desc_dec.h"
#include "ts_dec.h"
#include "ts.h"

/* **** Definitions **** */

#define GET_BITS(b) bitparser_get(bitparser_ctx, (b))
#define FLUSH_BITS(b) bitparser_flush(bitparser_ctx, (b))

/* **** Prototypes **** */

/* PAS specific data */
static psi_pas_ctx_t* psi_dec_pas(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, size_t size);

/* PMS specific */
static psi_pms_ctx_t* psi_dec_pms(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid, size_t size);

/* **** Implementations **** */

int psi_dec_get_next_section(fifo_ctx_t* ififo_ctx, log_ctx_t *log_ctx,
		uint8_t *ref_tscc, uint16_t pid,
		psi_section_ctx_t **ref_psi_section_ctx)
{
	int ret_code, end_code= STAT_ERROR;
	void *sect_buf= NULL;
	size_t sect_buf_size= 0;
	psi_section_ctx_t *psi_section_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments.
	 * Note: argument 'log_ctx' is allowed to be 'NULL'.
	 */
	CHECK_DO(ififo_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(ref_psi_section_ctx!= NULL, return STAT_ERROR);

	*ref_psi_section_ctx= NULL;

	/* Get complete section in buffer */
	ret_code= psi_dec_read_next_section(ififo_ctx, log_ctx, ref_tscc,
			&sect_buf, &sect_buf_size);
	if(ret_code!= STAT_SUCCESS) {
		if(ret_code== STAT_EOF) end_code= ret_code;
		goto end;
	}

	/* Parse (decode) section */
	ret_code= psi_dec_section((uint8_t*)sect_buf, sect_buf_size, pid,
			LOG_CTX_GET(), &psi_section_ctx);
	if(ret_code== STAT_ENODATA || psi_section_ctx== NULL) {
		end_code= STAT_ENODATA;
		goto end;
	}
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	*ref_psi_section_ctx= psi_section_ctx;
	psi_section_ctx= NULL; // Avoid double referencing
	end_code= STAT_SUCCESS;
end:
	if(psi_section_ctx!= NULL)
		psi_section_ctx_release(&psi_section_ctx);
	if(sect_buf!= NULL) {
		free(sect_buf);
		sect_buf= NULL;
	}
	return end_code;
}

int psi_dec_section(uint8_t *buf, size_t buf_size, uint16_t pid,
		log_ctx_t *log_ctx, psi_section_ctx_t **ref_psi_section_ctx)
{
	register uint8_t table_id;
	register uint8_t section_syntax_indicator;
	register uint16_t section_length;
	register uint16_t table_id_extension;
	register uint8_t section_number;
	register uint8_t last_section_number;
	psi_section_ctx_t *psi_section_ctx= NULL;
	bitparser_ctx_t *bitparser_ctx= NULL;
	void **sect_data= NULL;
	size_t sect_data_size= 0;
	int end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments.
	 * Note: argument 'log_ctx' is allowed to be 'NULL'.
	 */
	CHECK_DO(buf!= NULL, return STAT_ERROR);
	CHECK_DO(buf_size>8 && buf_size<= PSI_TABLE_MAX_SECTION_LEN,
			return STAT_ERROR);
	CHECK_DO(pid< TS_MAX_PID_VAL, return STAT_ERROR);
	CHECK_DO(ref_psi_section_ctx!= NULL, return STAT_ERROR);

	*ref_psi_section_ctx= NULL;

	/* Allocate PSI section context structure */
	psi_section_ctx= psi_section_ctx_allocate();
	CHECK_DO(psi_section_ctx!= NULL, goto end);

	/* Initialize bit-parser.
	 * NOTE: Due to parsing performance reasons, we work with buffer sizes
	 * multiple of sizeof(WORD_T) bytes. Thus, 'buf' allocation (externally
	 * performed) should consider this. In bit-parser initialization we assume
	 * allocation size is enough to overrun if applicable.
	 */
	bitparser_ctx= bitparser_open(buf, EXTEND_SIZE_TO_MULTIPLE(buf_size,
			sizeof(WORD_T)));
	CHECK_DO(bitparser_ctx!= NULL, goto end);

	/* Parse generic section fields */
	psi_section_ctx->table_id= table_id= GET_BITS(8);
	psi_section_ctx->section_syntax_indicator= section_syntax_indicator=
			GET_BITS(1);
	psi_section_ctx->indicator_1= GET_BITS(1);
	psi_section_ctx->reserved_1= GET_BITS(2);
	psi_section_ctx->section_length= section_length= GET_BITS(12);
	psi_section_ctx->table_id_extension= table_id_extension= GET_BITS(16);
	psi_section_ctx->reserved_2= GET_BITS(2);
	psi_section_ctx->version_number= GET_BITS(5);
	psi_section_ctx->current_next_indicator= GET_BITS(1);
	psi_section_ctx->section_number= section_number= GET_BITS(8);
	psi_section_ctx->last_section_number= last_section_number= GET_BITS(8);

	/* Check compliance: 'section_syntax_indicator' */
	if(section_syntax_indicator!= 1) {
		LOGE("Check compliance: Field 'section_syntax_indicator' "
				"should be set to '1'. PID= %u (0x%0x).\n", pid, pid);
		goto end;
	}

	/* Check compliance: 'section_length' */
	if(section_length< 9 || section_length> 1021) {
		LOGE("Check compliance: Invalid 'section_length' value %u "
				"(9<= length <= 1021). PID= %u (0x%0x).\n", section_length,
				pid, pid);
		goto end;
	}

	/* Check compliance: 'current_next_indicator' */
	if(psi_section_ctx->current_next_indicator== 0) {
		//LOGW("We skip this section and continue parsing until field "
		//		"'current_next_indicator'== '1'.\n"); //comment-me
		goto end;
	}

	/* Check compliance: 'section_number' */
	if(section_number> last_section_number) {
		LOGE("Check compliance: Invalid 'section_number' %u (should "
				"be minor than 'last_section_number' %u). PID= %u (0x%0x).\n",
				section_number, last_section_number, pid, pid);
		goto end;
	}

	sect_data= &psi_section_ctx->data;
	sect_data_size= section_length- (PSI_SECTION_FIXED_LEN- 3);

	/* Parse specific section data */
	switch(table_id) {
	case PSI_TABLE_PROGRAM_ASSOCIATION_SECTION:
		*sect_data= (void*)psi_dec_pas(log_ctx, bitparser_ctx, sect_data_size);
		break;
	case PSI_TABLE_TS_PROGRAM_MAP_SECTION:
		*sect_data= (void*)psi_dec_pms(log_ctx, bitparser_ctx, pid,
				sect_data_size);
		break;
	case PSI_DVB_SERVICE_DESCR_SECTION_ACTUAL:
		*sect_data= (void*)psi_dvb_dec_sds(log_ctx, bitparser_ctx, pid,
				sect_data_size);
		break;
	default:
		//LOGV("Unknown PSI table identifier %u (0x%0x). PID= %u (0x%0x).\n",
		//		table_id, table_id, pid, pid); //comment-me
		end_code= STAT_ENODATA;
		goto end;
	}
	if(psi_section_ctx->data== NULL) {
		//LOGW("Illegal section found (ID: %u; ID extension: %u). "
		//		"PID= %u (0x%0x).\n", table_id, table_id_extension,
		//		pid, pid); //comment-me
		goto end;
	}

	/* Parse CRC-32 */
	psi_section_ctx->crc_32= GET_BITS(32);
	//psi_section_ctx_trace((const psi_section_ctx_t*)
	//		psi_section_ctx); //comment-me

	*ref_psi_section_ctx= psi_section_ctx;
	psi_section_ctx= NULL; // Avoid double referencing
	end_code= STAT_SUCCESS;
end:
	psi_section_ctx_release(&psi_section_ctx);
	bitparser_close(&bitparser_ctx);
	return end_code;
}

int psi_dec_read_next_section(fifo_ctx_t* ififo_ctx, log_ctx_t *log_ctx,
		uint8_t *ref_tscc, void **buf, size_t *count)
{
	uint8_t *payload;
	uint8_t payload_size;
	uint8_t pointer_field;
	uint8_t table_id;
	int i, ret_code, end_code= STAT_ERROR;
	ts_ctx_t* ts_ctx= NULL;
	uint8_t *section_data= NULL;
	size_t section_size= 0; // Parsed size
	int flag_section_length_parsed= 0, flag_section_completed= 0;
	uint16_t section_length= 0; // Set to "not specified"; update below.
	uint16_t psi_pid= 0xFFFF; // Initialize to invalid.
	const size_t psi_table_max_section_len_aligned= EXTEND_SIZE_TO_MULTIPLE(
			PSI_TABLE_MAX_SECTION_LEN, CTX_S_BASE_ALIGN);
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(ififo_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(ref_tscc!= NULL, return STAT_ERROR);
	CHECK_DO(buf!= NULL, return STAT_ERROR);
	CHECK_DO(count!= NULL, return STAT_ERROR);

	*buf= NULL;
	*count= 0;

	/* Synchronize section.
	 * Get next mpeg2-TS packet such:
	 * - payload unit start indicator is set
	 * - payload is effectively present
	 * Note that input FIFO provide packet with a single PID value.
	 */
	/* When the payload of the transport stream packet contains transport
	 * stream section data, the 'payload_unit_start_indicator' has the
	 * following significance: if the transport stream packet carries the
	 * first byte of a section, the payload_unit_start_indicator value shall
	 * be '1', indicating that the first byte of the payload of this transport
	 * stream packet carries the pointer_field. If the transport stream packet
	 * does not carry the first byte of a section, the
	 * payload_unit_start_indicator value shall be '0', indicating that there
	 * is no pointer_field in the payload.
	 * It is important to note that a PSI section start may be found at any
	 * point of the TS packet payload, so it is possible that the
	 * 'section_length' field -or any other- may "fall" in the next TS packet.
	 */
	while(ts_ctx== NULL ||
			!ts_ctx->payload_unit_start_indicator ||
			!ts_ctx->contains_payload) {
		ts_ctx_release(&ts_ctx);
		ret_code= ts_dec_get_next_packet(ififo_ctx, log_ctx, ref_tscc, &ts_ctx);
		if(ret_code!= STAT_SUCCESS) {
			if(ret_code== STAT_EOF) end_code= STAT_EOF;
			goto end;
		}
	}

	psi_pid= ts_ctx->pid;
	payload= ts_ctx->payload;
	payload_size= ts_ctx->payload_size;
	CHECK_DO(payload!= NULL && payload_size> 0, goto end);

	/* First 8-bits of payload are the so-called 'pointer_field' */
	pointer_field= payload[0];
	CHECK_DO(payload_size>= pointer_field+ 1, goto end);
	//LOGV("Pointer field value: %d\n", pointer_field); //comment-me

	/* Check first byte ('table_id' field) of section */
	if((1+ pointer_field)>= payload_size) {
		LOGE("Invalid pointer field value while synchronising next PSI "
				"table. Pointer field out of bounds (pointer: %u, "
				"payload size: %u). PID= %u (0x%0x).\n", pointer_field,
				payload_size, psi_pid, psi_pid);
		goto end;
	}
	table_id= payload[1+ pointer_field];
	if(table_id== 0xFF) {
		/* 'table_id value 0xFF is forbidden'; discard */
		LOGE("Forbidden 'table_id' value 0xFF parsed in PSI section. PID= %u "
				"(0x%0x).\n", psi_pid, psi_pid);
		goto end;
	}

	/* Copy first bytes of PSI section taking into account 'pointer_field' */
	section_size+= (payload_size- 1- pointer_field);
	CHECK_DO(section_size<= PSI_TABLE_MAX_SECTION_LEN, goto end);
	CHECK_DO(SIZE_IS_MULTIPLE(PSI_TABLE_MAX_SECTION_LEN, sizeof(WORD_T)),
			goto end);
	section_data= (uint8_t*)malloc(psi_table_max_section_len_aligned);
	CHECK_DO(section_data!= NULL, goto end);
	memset(section_data, 0xFF, psi_table_max_section_len_aligned);
	memcpy(section_data, &payload[1+ pointer_field], section_size);

	/* Check if we have parsed enough bytes to get 'section_length' */
	if(flag_section_length_parsed== 0 && section_size>= 3) {
		section_length = (uint16_t)section_data[2];
		section_length|= (((uint16_t)section_data[1])& 0x000F)<< 8;
		flag_section_length_parsed= 1;
	}

	/* Check if section is completed based on specified length */
	if(flag_section_length_parsed!= 0 && (section_length+ 3)<= section_size) {
		flag_section_completed= 1;
	}

	/* Copy rest of the section */
	while(flag_section_completed== 0) {
		uint8_t payload_unit_start_indicator;
		uint8_t *ts_pkt;
		size_t ts_pkt_size;

		/* Show (but do not flush from FIFO) next TS-packet to check only if
		 * 'payload_unit_start_indicator' flag is set.
		 */
		ret_code= fifo_show(ififo_ctx, (void**)&ts_pkt, &ts_pkt_size);
		if(ret_code!= STAT_SUCCESS) {
			if(ret_code== STAT_EAGAIN)
				end_code= STAT_EOF; // FIFO unblocked; we are requested to exit.
			goto end;
		}
		CHECK_DO(ts_pkt!= NULL && ts_pkt[0]== 0x47&& ts_pkt_size== TS_PKT_SIZE,
				goto end);

		/* If new section is detected in next TS; exit loop. */
		if((payload_unit_start_indicator= ts_pkt[1]& 0x40)!= 0) {
			if(ts_ctx!= NULL)
				ts_ctx_release(&ts_ctx);
			ret_code= ts_dec_packet(ts_pkt, log_ctx, &ts_ctx);
			if(ret_code!= STAT_SUCCESS)
				goto end;

			if(ts_ctx->payload== NULL || ts_ctx->payload_size== 0) {
				LOGE("Payload unit start expected, but transport packet does "
						"not carry payload. PID= %u (0x%0x).\n",
						psi_pid, psi_pid);
				goto end;
			}
			payload= &ts_ctx->payload[1]; // skip 'pointer_field'
			payload_size= ts_ctx->payload[0]; // value of 'pointer_field'
			CHECK_DO(payload!= NULL && payload_size>= 0, goto end);
			if((1+ payload_size)>= ts_ctx->payload_size) {
				LOGE("Invalid pointer field value while synchronising next PSI "
						"table. Pointer field out of bounds (pointer: %u, "
						"payload size: %u). PID= %u (0x%0x).\n", payload_size,
						ts_ctx->payload_size, psi_pid, psi_pid);
				goto end;
			}

			/* Reallocate section buffer and copy section remainder. */
			if(payload_size> 0) {
				CHECK_DO(section_size+ payload_size<= PSI_TABLE_MAX_SECTION_LEN,
						goto end);
				memcpy(&section_data[section_size], payload, payload_size);
				section_size+= payload_size;
			}

			/* Get/check 'section_length' */
			CHECK_DO(section_size>= 3, goto end);
			if(flag_section_length_parsed== 0) {
				section_length = (uint16_t)section_data[2];
				section_length|= (((uint16_t)section_data[1])& 0x000F)<< 8;
				flag_section_length_parsed= 1;
			}

			flag_section_completed= 1;
			break; // Exit loop.
		}

		/* Get (flushing from buffer) the new TS packet. */
		if(ts_ctx!= NULL)
			ts_ctx_release(&ts_ctx);
		ret_code= ts_dec_get_next_packet(ififo_ctx, log_ctx, ref_tscc, &ts_ctx);
		if(ret_code!= STAT_SUCCESS) {
			if(ret_code== STAT_EOF) end_code= STAT_EOF;
			goto end;
		}
		if(ts_ctx== NULL || !ts_ctx->contains_payload) {
			continue;
		}

		payload= ts_ctx->payload;
		payload_size= ts_ctx->payload_size;
		CHECK_DO(payload!= NULL && payload_size> 0, goto end);

		/* Reallocate section buffer and copy TS packet payload. */
		CHECK_DO(section_size+ payload_size<= PSI_TABLE_MAX_SECTION_LEN,
				goto end);
		memcpy(&section_data[section_size], payload, payload_size);
		section_size+= payload_size;


		/* Check if we have parsed enough bytes to get 'section_length' */
		if(flag_section_length_parsed== 0 && section_size>= 3) {
			section_length = (uint16_t)section_data[2];
			section_length|= (((uint16_t)section_data[1])& 0x000F)<< 8;
			flag_section_length_parsed= 1;
		}

		/* Check if section is completed based on specified length */
		if(flag_section_length_parsed!= 0 &&
				(section_length+ 3)<= section_size) {
			flag_section_completed= 1;
		}
	}
	ASSERT(flag_section_completed== 1);

	/* Check compliance: 'section_size'. and stuffing bytes. */
	CHECK_DO(section_size<= PSI_TABLE_MAX_SECTION_LEN &&
			(section_length+ 3)<= section_size, goto end);
	if((section_length+ 3)< section_size) {
		/* Stuffing bytes should exist at the end of the section. */
		/* Check compliance: stuffing bytes:
		 * Within a Transport Stream, packet stuffing bytes of value 0xFF may
		 * be found in the payload of Transport Stream packets carrying PSI
		 * and/or private_sections only after the last byte of a section. In
		 * this case all bytes until the end of the Transport Stream packet
		 * shall also be stuffing bytes of value 0xFF. In such a case, the
		 * payload of the next Transport Stream packet with the same PID value
		 * shall begin with a pointer_field of value 0x00 indicating that the
		 * next section starts immediately thereafter.
		 * If the value 0xFF was present, then the following TS Packet of
		 * the same PID either:
		 * (1) consists only of adaptation_field, or
		 * (2) has the payload_unit_start_indicator set to '1', has the
		 * pointer_field set to ‘0’ and then starts a new section.
		 */
		for(i= (section_length+ 3); i< section_size; i++) {
			if(section_data[i]!= 0xFF) {
				LOGEV("Unexpected PSI section padding conditions: "
						"illegal padding bytes not set to value 0xFF or parsed "
						"PSI section is longer than declared. "
						"PID= %u (0x%0x).\n", psi_pid, psi_pid);
				break;
			}
		}
		// TODO: Check conditions (1) and (2) mentioned above.
	}

	/* Check compliance: CRC-32. */
	if(crc_32_mpeg2(section_data, section_length+ 3)!= 0) {
		LOGEV("Check compliance: Inconsistent CRC-32 in PSI section. "
				"PID= %u (0x%0x).\n", psi_pid, psi_pid);
		goto end;
	}

	*count= section_length+ 3; // We do not report stuffing size
	*buf= (void*)section_data;
	section_data= NULL; // Avoid double referencing
	end_code= STAT_SUCCESS;
	//LOGV("New PSI section read (length: %d)\n", (int)*count); //comment-me
end:
	ts_ctx_release(&ts_ctx);
	if(section_data!= NULL)
		free(section_data);
	if(end_code!= STAT_SUCCESS) {
		if(*buf!= NULL) {
			free(*buf);
			*buf= NULL;
		}
	}
	return end_code;
}

static psi_pas_ctx_t* psi_dec_pas(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, size_t size)
{
	psi_pas_ctx_t *psi_pas_ctx= NULL;
	psi_pas_prog_ctx_t *psi_pas_prog_ctx_nth= NULL;
	int i, ret_code, end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(bitparser_ctx!= NULL, return NULL);
	CHECK_DO(size> 0, return NULL);

	/* Allocate PMS context structure */
	psi_pas_ctx= psi_pas_ctx_allocate();
	CHECK_DO(psi_pas_ctx!= NULL, goto end);

	/* Loop through the 'N' programs fields (32-bit per loop) */
	for(i= 0; i< (size>> 2); i++) {
		/* Allocate PAS specific data context structure */
		psi_pas_prog_ctx_nth= psi_pas_prog_ctx_allocate();
		CHECK_DO(psi_pas_prog_ctx_nth!= NULL, goto end);

		/* Parse specific data */
		psi_pas_prog_ctx_nth->program_number= GET_BITS(16);
		FLUSH_BITS(3); // "reserved"
		psi_pas_prog_ctx_nth->reference_pid= GET_BITS(13);

		/* Insert node in list of PAS specific data context structures */
		ret_code= llist_insert_nth(&psi_pas_ctx->psi_pas_prog_ctx_llist, i,
				psi_pas_prog_ctx_nth);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		psi_pas_prog_ctx_nth= NULL; // avoid freeing at the end of function.
	}

	end_code= STAT_SUCCESS;
end:
	psi_pas_prog_ctx_release(&psi_pas_prog_ctx_nth);

	if(end_code!= STAT_SUCCESS)
		psi_pas_ctx_release(&psi_pas_ctx);

	return psi_pas_ctx;
}

static psi_pms_ctx_t* psi_dec_pms(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid, size_t size)
{
	psi_pms_ctx_t *psi_pms_ctx= NULL;
	psi_pms_es_ctx_t *psi_pms_es_ctx_nth= NULL;
	int i, program_info_length, es_data_length, es_info_length, ret_code,
	end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(bitparser_ctx!= NULL, return NULL);
	CHECK_DO(size> 0, return NULL);

	/* Allocate Program Map Section specific data context structure */
	psi_pms_ctx= psi_pms_ctx_allocate();
	CHECK_DO(psi_pms_ctx!= NULL, goto end);

	/* Parse fields */
	FLUSH_BITS(3); // "reserved"
	psi_pms_ctx->pcr_pid= GET_BITS(13);
	FLUSH_BITS(4); // "reserved"
	program_info_length= psi_pms_ctx->program_info_length= GET_BITS(12);

	/* Get 'N' program descriptors data (type 'psi_desc_ctx_t'). */
	while(program_info_length> 0) {
		psi_desc_ctx_t *psi_desc_ctx= psi_desc_dec(log_ctx, bitparser_ctx,
				pid, program_info_length);
		if(psi_desc_ctx== NULL) {
			//LOGE("Illegal program information descriptor found.\n");
			goto end;
		}
		ret_code= llist_push(&psi_pms_ctx->psi_desc_ctx_llist,
				(void*)psi_desc_ctx);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		program_info_length-=
				PSI_DESC_FIXED_LEN+ psi_desc_ctx->descriptor_length;
	}
	CHECK_DO(program_info_length== 0, goto end);

	/* Get 'N1' program elementary-stream information data */
	es_data_length= size; // pms specific data length
	es_data_length-= (4+ psi_pms_ctx->program_info_length); // ES data length
	for(i= 0; es_data_length> 0; i++) {

		/* Allocate program ES context structure */
		psi_pms_es_ctx_nth= psi_pms_es_ctx_allocate();
		CHECK_DO(psi_pms_es_ctx_nth!= NULL, goto end);

		/* Parse fields */
		CHECK_DO(es_data_length>= 5, goto end);
		psi_pms_es_ctx_nth->stream_type= GET_BITS(8);
		FLUSH_BITS(3); //reserved
		psi_pms_es_ctx_nth->elementary_PID= GET_BITS(13);
		FLUSH_BITS(4); //reserved
		es_info_length= psi_pms_es_ctx_nth->es_info_length= GET_BITS(12);

		if(es_data_length< (5+ es_info_length)) {
			LOGEV("Inconsistent ES descriptors information length: "
					"sum of declared descriptors length exceeds declared "
					"section length. PID= %u (0x%0x)\n", pid, pid);
			goto end;
		}

		/* Get 'N2' ES descriptors data */
		//LOGV("\n> ES descriptors (ES PID: %u):\n",
		//		psi_pms_es_ctx_nth->elementary_PID); //comment-me
		while(es_info_length> 0) {
			psi_desc_ctx_t *psi_desc_ctx= psi_desc_dec(log_ctx, bitparser_ctx,
					pid, es_info_length);
			if(psi_desc_ctx== NULL) {
				//LOGEV("Illegal ES information descriptor found "
				//		"(PID: %u). Rest of ES descriptors can not be parsed "
				//		"as are out of synchronization (synchronizing to next "
				//		"ES data).\n", psi_pms_es_ctx_nth->elementary_PID);
				/* Note that 2 bytes, corresponding to 'descriptor_tag' and
				 * 'descriptor_length', were already consumed in function
				 * 'psi_desc_dec()'.
				 */
				FLUSH_BITS((es_info_length- 2)<<3);
				es_info_length= 0;
				break;
			}
			ret_code= llist_push(&psi_pms_es_ctx_nth->psi_desc_ctx_llist,
					(void*)psi_desc_ctx);
			CHECK_DO(ret_code== STAT_SUCCESS,
					psi_desc_ctx_release(&psi_desc_ctx); goto end);
			es_info_length-= PSI_DESC_FIXED_LEN+
					psi_desc_ctx->descriptor_length;
		}
		CHECK_DO(es_info_length== 0, goto end);

		/* Update remaining ES data length */
		es_data_length-= (5+ psi_pms_es_ctx_nth->es_info_length);

		/* Insert node in list of elementary stream information context
		 * structures.
		 */
		ret_code= llist_insert_nth(&psi_pms_ctx->psi_pms_es_ctx_llist, i,
				psi_pms_es_ctx_nth);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		psi_pms_es_ctx_nth= NULL; // avoid freeing at the end of function.
	}

	/* Check we have consumed all bytes consistently */
	CHECK_DO(es_data_length== 0, goto end);

	end_code= STAT_SUCCESS;
end:
	if(psi_pms_es_ctx_nth!= NULL)
		psi_pms_es_ctx_release(&psi_pms_es_ctx_nth);
	if(end_code!= STAT_SUCCESS)
		psi_pms_ctx_release(&psi_pms_ctx);
	return psi_pms_ctx;
}
