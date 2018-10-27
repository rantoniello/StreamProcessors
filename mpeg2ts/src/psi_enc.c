/*
 * Copyright (c) 2015, 2016, 2017 Rafael Antoniello
 *
 * This file is part of StreamProcessor.
 *
 * StreamProcessor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StreamProcessor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with StreamProcessor.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file psi_enc.c
 * @Author Rafael Antoniello
 */

#include "psi_enc.h"

#include <stdlib.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/llist.h>
#include <libmediaprocsutils/crc_32_mpeg2.h>

#include "psi.h"
#include "psi_desc.h"
#include "psi_desc_enc.h"
#include "ts.h"
#include "ts_enc.h"

/* **** Definitions **** */

/* **** Prototypes **** */

static int psi_enc_pas(const psi_pas_ctx_t *psi_pas_ctx, uint8_t *buf,
		size_t sect_data_size, uint16_t pid, log_ctx_t *log_ctx);
static int psi_enc_pms(const psi_pms_ctx_t *psi_pms_ctx, uint8_t *buf,
		size_t sect_data_size, uint16_t pid, log_ctx_t *log_ctx);

/* **** Implementations **** */

int psi_section_ctx_enc(const psi_section_ctx_t *psi_section_ctx_arg,
		uint16_t pid, log_ctx_t *log_ctx, void **ref_buf, size_t *ref_buf_size)
{
	register uint8_t table_id;
	register uint16_t section_length;
	register uint8_t  section_number;
	register uint8_t  last_section_number;
	register uint32_t crc_32;
	uint8_t sect_buf[PSI_TABLE_MPEG_MAX_SECTION_LEN];
	int ret_code, end_code= STAT_ERROR;
	psi_section_ctx_t psi_section_ctx= {0};
	void *sect_data= NULL;
	size_t sect_data_size= 0;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments.
	 * Note: argument 'log_ctx' is allowed to be NULL.
	 */
	CHECK_DO(psi_section_ctx_arg!= NULL, return STAT_ERROR);
	CHECK_DO(pid< TS_MAX_PID_VAL, return STAT_ERROR);
	CHECK_DO(ref_buf!= NULL, return STAT_ERROR);

	*ref_buf= NULL;
	*ref_buf_size= 0;

	/* Re-compose and check some fields of the section */
	psi_section_ctx.table_id= table_id= psi_section_ctx_arg->table_id;
	psi_section_ctx.section_syntax_indicator= 1;
	psi_section_ctx.indicator_1= 0;
	psi_section_ctx.reserved_1= 0;
	section_length= psi_section_ctx_arg->section_length;
	psi_section_ctx.section_length= section_length;
	psi_section_ctx.table_id_extension= psi_section_ctx_arg->table_id_extension;
	psi_section_ctx.reserved_2= 0;
	psi_section_ctx.version_number= psi_section_ctx_arg->version_number;
	psi_section_ctx.current_next_indicator= 1;
	section_number= psi_section_ctx_arg->section_number;
	psi_section_ctx.section_number= section_number;
	last_section_number= psi_section_ctx_arg->last_section_number;
	psi_section_ctx.last_section_number= last_section_number;
	psi_section_ctx.data= psi_section_ctx_arg->data;
	CHECK_DO(psi_section_ctx.data!= NULL, goto end);
	//psi_section_ctx.crc_32 // Re-computed below

	/* Check compliance: 'section_syntax_indicator' */
	if(psi_section_ctx_arg->section_syntax_indicator!= 1)
		LOGW("Check compliance: Field 'section_syntax_indicator' should be set "
				"to '1' (Automatically corrected while encoding PSI section). "
				"PID= %u (0x%0x).\n", pid, pid);

	/* Check compliance: 'section_length' */
	if(section_length< 9 || section_length> 1021) {
		LOGE("Check compliance: Invalid 'section_length' value %u "
				"(9<= length <= 1021). PID= %u (0x%0x).\n", section_length,
				pid, pid);
		goto end;
	}

	/* Check compliance: 'current_next_indicator' */
	if(psi_section_ctx_arg->current_next_indicator== 0)
		LOGW("Forcing field 'current_next_indicator'== '1'. "
				"PID= %u (0x%0x).\n", pid, pid);

	/* Check compliance: 'section_number' */
	if(section_number> last_section_number) {
		LOGE("Check compliance: Invalid 'section_number' %u (should "
				"be minor than 'last_section_number' %u). PID= %u (0x%0x).\n",
				section_number, last_section_number, pid, pid);
		goto end;
	}

	/* Put section in byte buffer */
	memset(sect_buf, 0xFF, sizeof(sect_buf));
	sect_buf[0] = psi_section_ctx.table_id;
	sect_buf[1] = psi_section_ctx.section_syntax_indicator<< 7;
	sect_buf[1]|= psi_section_ctx.indicator_1<< 6;
	sect_buf[1]|= psi_section_ctx.reserved_1<< 4;
	sect_buf[1]|= psi_section_ctx.section_length>> 8;
	sect_buf[2] = psi_section_ctx.section_length& 0xFF;
	sect_buf[3] = psi_section_ctx.table_id_extension>> 8;
	sect_buf[4] = psi_section_ctx.table_id_extension& 0xFF;
	sect_buf[5] = psi_section_ctx.reserved_2<< 6;
	sect_buf[5]|= psi_section_ctx.version_number<< 1;
	sect_buf[5]|= psi_section_ctx.current_next_indicator;
	sect_buf[6] = psi_section_ctx.section_number;
	sect_buf[7] = psi_section_ctx.last_section_number;

	sect_data= psi_section_ctx.data;
	sect_data_size= section_length- (PSI_SECTION_FIXED_LEN- 3);

	/* Parse specific section data */
	switch(table_id) {
	case PSI_TABLE_PROGRAM_ASSOCIATION_SECTION:
		ret_code= psi_enc_pas((const psi_pas_ctx_t*)sect_data,
				(uint8_t*)sect_buf, sect_data_size, pid, LOG_CTX_GET());
		CHECK_DO(ret_code== STAT_SUCCESS, goto end)
		break;
	case PSI_TABLE_TS_PROGRAM_MAP_SECTION:
		ret_code= psi_enc_pms((const psi_pms_ctx_t*)sect_data,
				(uint8_t*)sect_buf, sect_data_size, pid, LOG_CTX_GET());
		CHECK_DO(ret_code== STAT_SUCCESS, goto end)
		break;
	default:
		LOGD("Unknown or unsupported PSI table identifier. PID= %u (0x%0x).\n",
				pid, pid);
		goto end;
	}
	CHECK_DO(section_length+ 3<= sizeof(sect_buf), goto end);

	psi_section_ctx.crc_32= crc_32= crc_32_mpeg2(sect_buf,
			section_length+ 3- 4); // include all bytes except CRC
	sect_buf[sect_data_size+ 8   ]= (crc_32                      )>> 24;
	sect_buf[sect_data_size+ 8+ 1]= (crc_32& (uint32_t)0x00FF0000)>> 16;
	sect_buf[sect_data_size+ 8+ 2]= (crc_32& (uint32_t)0x0000FF00)>> 8;
	sect_buf[sect_data_size+ 8+ 3]= (crc_32& (uint32_t)0x000000FF);

	*ref_buf= malloc(section_length+ 3);
	CHECK_DO(*ref_buf!= NULL, goto end);
	memcpy(*ref_buf, sect_buf, section_length+ 3);
	*ref_buf_size= section_length+ 3;
	end_code= STAT_SUCCESS;
end:
	return end_code;
}

static int psi_enc_pas(const psi_pas_ctx_t *psi_pas_ctx, uint8_t *buf,
		size_t sect_data_size, uint16_t pid, log_ctx_t *log_ctx)
{
	llist_t *n;
	register int end_code= STAT_ERROR;
	register int byte_idx= 8; // Add PSI section fixed bytes
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(psi_pas_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(buf!= NULL, return STAT_ERROR);

	/* Encode Program Association Section (PAS) specific data context
	 * structure.
	 */
	for(n= psi_pas_ctx->psi_pas_prog_ctx_llist; n!= NULL; n= n->next) {
		psi_pas_prog_ctx_t *psi_pas_prog_ctx;

		psi_pas_prog_ctx= (psi_pas_prog_ctx_t*)n->data;
		CHECK_DO(psi_pas_prog_ctx!= NULL, continue);

		CHECK_DO(byte_idx+ 4< PSI_TABLE_MPEG_MAX_SECTION_LEN, goto end);
		buf[byte_idx++]= psi_pas_prog_ctx->program_number>> 8;
		buf[byte_idx++]= psi_pas_prog_ctx->program_number& 0xFF;
		buf[byte_idx++]= psi_pas_prog_ctx->reference_pid>> 8;
		buf[byte_idx++]= psi_pas_prog_ctx->reference_pid& 0xFF;
	}

	end_code= STAT_SUCCESS;
end:
	return end_code;
}

static int psi_enc_pms(const psi_pms_ctx_t *psi_pms_ctx, uint8_t *buf,
		size_t sect_data_size, uint16_t pid, log_ctx_t *log_ctx)
{
	llist_t *n;
	register uint16_t pcr_pid;
	register uint16_t program_info_length;
	register int ret_code, byte_cnt_aux, end_code= STAT_ERROR;
	register int byte_idx= 8; // Add PSI section fixed bytes
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(psi_pms_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(buf!= NULL, return STAT_ERROR);

	/* Encode Program Map Section (PMS) specific data context structure */
	pcr_pid= psi_pms_ctx->pcr_pid;
	buf[byte_idx++] = (pcr_pid>> 8)& 0x1F;
	buf[byte_idx++] = pcr_pid& 0xFF;
	program_info_length= psi_pms_ctx->program_info_length;
	buf[byte_idx++] = (program_info_length>> 8)& 0xF;
	buf[byte_idx++] = program_info_length& 0xFF;

	/* Encode list of program information descriptors */
	for(n= psi_pms_ctx->psi_desc_ctx_llist, byte_cnt_aux= 0; n!= NULL;
			n= n->next) {
		psi_desc_ctx_t *psi_desc_ctx;

		psi_desc_ctx= (psi_desc_ctx_t*)n->data;
		CHECK_DO(psi_desc_ctx!= NULL, continue);

        ret_code= psi_desc_enc(LOG_CTX_GET(), psi_desc_ctx, &buf[byte_idx]);
        CHECK_DO(ret_code== STAT_SUCCESS, goto end);

        byte_cnt_aux+= PSI_DESC_FIXED_LEN+ psi_desc_ctx->descriptor_length;
        byte_idx+= PSI_DESC_FIXED_LEN+ psi_desc_ctx->descriptor_length;
	}
	if(program_info_length!= byte_cnt_aux) { // Check descriptors length
		LOGE("Illegal program information size: %u bytes parsed "
				"but %u were declared.\n", byte_cnt_aux, program_info_length);
		goto end;
	}
	CHECK_DO(byte_idx== (8+ 4+ program_info_length), goto end);

	/* Encode list of elementary stream information context structures */
	for(n= psi_pms_ctx->psi_pms_es_ctx_llist; n!= NULL; n= n->next) {
		llist_t *n2;
		register uint16_t elementary_PID;
		register uint16_t es_info_length;
		psi_pms_es_ctx_t *psi_pms_es_ctx;

		psi_pms_es_ctx= (psi_pms_es_ctx_t*)n->data;
		CHECK_DO(psi_pms_es_ctx!= NULL, continue);

		buf[byte_idx++]= psi_pms_es_ctx->stream_type;
    	elementary_PID= psi_pms_es_ctx->elementary_PID;
    	buf[byte_idx++]= (elementary_PID>> 8)& 0x1F;
    	buf[byte_idx++]= elementary_PID& 0xFF;
    	es_info_length= psi_pms_es_ctx->es_info_length;
    	buf[byte_idx++]= (es_info_length>> 8)& 0xF;
    	buf[byte_idx++]= es_info_length& 0xFF;

    	/* Encode list of Elementary Stream (ES) information descriptors */
    	for(n2= psi_pms_es_ctx->psi_desc_ctx_llist, byte_cnt_aux= 0; n2!= NULL;
    			n2= n2->next) {
    		psi_desc_ctx_t *psi_desc_ctx;

    		psi_desc_ctx= (psi_desc_ctx_t*)n2->data;
    		CHECK_DO(psi_desc_ctx!= NULL, continue);

            ret_code= psi_desc_enc(LOG_CTX_GET(), psi_desc_ctx, &buf[byte_idx]);
            CHECK_DO(ret_code== STAT_SUCCESS, goto end);

            byte_cnt_aux+= PSI_DESC_FIXED_LEN+ psi_desc_ctx->descriptor_length;
            byte_idx+= PSI_DESC_FIXED_LEN+ psi_desc_ctx->descriptor_length;
    	}
    	if(es_info_length!= byte_cnt_aux) { // Check descriptors length
    		LOGE("Illegal elementary stream information size: %u bytes parsed "
    				"but %u were declared. PID= %u (0x%0x).\n", byte_cnt_aux,
					es_info_length, elementary_PID, elementary_PID);
    		goto end;
    	}
	}

	/* Check encoded section length */
	if(sect_data_size!= (byte_idx- 8/*exclude first bytes*/)) {
		LOGE("Illegal PSI-PMS section size parsed "
				"(declared value= %u; parsed value= %d). PID= %u (0x%0x).\n",
				sect_data_size, byte_idx- 8, pid, pid);
		goto end;
	}

	end_code= STAT_SUCCESS;
end:
	return end_code;
}
