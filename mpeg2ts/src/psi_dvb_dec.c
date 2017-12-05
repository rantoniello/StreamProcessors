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
 * @file psi_dvb_dec.c
 * @Author Rafael Antoniello
 */

#include "psi_dvb_dec.h"

#include <stdlib.h>
#include <string.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/llist.h>
#include <libmediaprocsutils/bitparser.h>
#include "psi.h"
#include "psi_desc.h"
#include "psi_desc_dec.h"
#include "psi_dvb.h"

/* **** Definitions **** */

#define GET_BITS(b) bitparser_get(bitparser_ctx, (b))
#define FLUSH_BITS(b) bitparser_flush(bitparser_ctx, (b))

/* **** Prototypes **** */

static psi_dvb_sds_prog_ctx_t* psi_dvb_sds_prog(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid);

/* **** Implementations **** */

psi_dvb_sds_ctx_t* psi_dvb_dec_sds(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid, size_t size)
{
	psi_dvb_sds_ctx_t *psi_dvb_sds_ctx= NULL;
	int service_data_len, ret_code, end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments.
	 * Note: Argument 'log_ctx' is allowed to be 'NULL'.
	 */
	CHECK_DO(bitparser_ctx!= NULL, return NULL);
	CHECK_DO(size> 0, return NULL);

	/* Allocate SDS context structure */
	psi_dvb_sds_ctx= psi_dvb_sds_ctx_allocate();
	CHECK_DO(psi_dvb_sds_ctx!= NULL, goto end);

	/* Parse fields */
	psi_dvb_sds_ctx->original_network_id= GET_BITS(16);
	FLUSH_BITS(8); // "reserved"

	/* Get services data list (type 'psi_dvb_sds_prog_ctx_t') */
	service_data_len= size- 3; // 3 bytes: 'original_network_id'+ 'reserved'.
	CHECK_DO(service_data_len<= PSI_TABLE_MPEG_MAX_SECTION_LEN, goto end);
	while(service_data_len> 0) {
		psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx= psi_dvb_sds_prog(log_ctx,
				bitparser_ctx, pid);
		ret_code= llist_push(&psi_dvb_sds_ctx->psi_dvb_sds_prog_ctx_llist,
				(void*)psi_dvb_sds_prog_ctx);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		service_data_len-= PSI_DVB_SDS_PROG_FIXED_LEN+
				psi_dvb_sds_prog_ctx->descriptors_loop_length;
	}
	CHECK_DO(service_data_len== 0, goto end);

	end_code= STAT_SUCCESS;

end:
	if(end_code!= STAT_SUCCESS)
		psi_dvb_sds_ctx_release(&psi_dvb_sds_ctx);
	return psi_dvb_sds_ctx;
}

static psi_dvb_sds_prog_ctx_t* psi_dvb_sds_prog(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid)
{
	psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx= NULL;
	int desc_loop_length, ret_code, end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(bitparser_ctx!= NULL, return NULL);

	/* Allocate "single service description information" context structure */
	psi_dvb_sds_prog_ctx= psi_dvb_sds_prog_ctx_allocate();
	CHECK_DO(psi_dvb_sds_prog_ctx!= NULL, goto end);

	/* Parse fields */
	psi_dvb_sds_prog_ctx->service_id= GET_BITS(16);
	FLUSH_BITS(6); //reserved
	psi_dvb_sds_prog_ctx->EIT_schedule_flag= GET_BITS(1);
	psi_dvb_sds_prog_ctx->EIT_present_following_flag= GET_BITS(1);
	psi_dvb_sds_prog_ctx->running_status= GET_BITS(3);
	psi_dvb_sds_prog_ctx->free_CA_mode= GET_BITS(1);
	desc_loop_length= psi_dvb_sds_prog_ctx->descriptors_loop_length=
			GET_BITS(12);

	/* Get service descriptors data (type 'psi_desc_ctx_t'). */
	CHECK_DO(desc_loop_length<= PSI_TABLE_MPEG_MAX_SECTION_LEN, goto end);
	while(desc_loop_length> 0) {
		psi_desc_ctx_t *psi_desc_ctx= psi_desc_dec(log_ctx, bitparser_ctx, pid,
				desc_loop_length);
		CHECK_DO(psi_desc_ctx!= NULL, goto end);
		ret_code= llist_push(&psi_dvb_sds_prog_ctx->psi_desc_ctx_llist,
				(void*)psi_desc_ctx);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		desc_loop_length-= PSI_DESC_FIXED_LEN+ psi_desc_ctx->descriptor_length;
	}
	CHECK_DO(desc_loop_length== 0, goto end);

	end_code= STAT_SUCCESS;

end:
	if(end_code!= STAT_SUCCESS)
		psi_dvb_sds_prog_ctx_release(&psi_dvb_sds_prog_ctx);
	return psi_dvb_sds_prog_ctx;
}
