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
 * @file psi_desc_enc.c
 * @Author Rafael Antoniello
 */

#include "psi_desc_enc.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/llist.h>
#include "psi_desc.h"

/* **** Definitions **** */

/* **** Prototypes **** */

static int psi_desc_enc_dvb_service(log_ctx_t *log_ctx,
		const psi_desc_dvb_service_ctx_t *psi_desc_dvb_service_ctx,
		void *buf, uint8_t descriptor_length);

static int psi_desc_enc_dvb_subt(log_ctx_t *log_ctx,
		const psi_desc_dvb_subt_ctx_t *psi_desc_dvb_subt_ctx, void *buf,
		uint8_t descriptor_length);

/* **** Implementations **** */

int psi_desc_enc(log_ctx_t *log_ctx, const psi_desc_ctx_t *psi_desc_ctx,
		void *buf)
{
	uint8_t descriptor_tag;
	uint8_t descriptor_length;
	void *data;
	uint8_t *data_buf;
	int ret_code, end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments.
	 * Note: Argument 'log_ctx' is allowed to be 'NULL'.
	 */
	CHECK_DO(psi_desc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(buf!= NULL, return STAT_ERROR);

	LOGD("Entering '%s'\n", __func__);

	/* Put descriptor in given byte buffer */
	((uint8_t*)buf)[0]= descriptor_tag= psi_desc_ctx->descriptor_tag;
	((uint8_t*)buf)[1]= descriptor_length= psi_desc_ctx->descriptor_length;
	data_buf= &((uint8_t*)buf)[2];
	data= psi_desc_ctx->data;

	/* Check if we have specific data to process */
	if(descriptor_length== 0 && data== NULL) { // No specific data to encode
		end_code= STAT_SUCCESS;
		goto end;
	}
	CHECK_DO(data!= NULL && descriptor_length> 0, goto end);

	switch(descriptor_tag) {
	case PSI_DESC_TAG_DVB_SERVICE:
		ret_code= psi_desc_enc_dvb_service(log_ctx, data, data_buf,
				descriptor_length);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		break;
	case PSI_DESC_TAG_DVB_SUBT:
		ret_code= psi_desc_enc_dvb_subt(log_ctx, data, data_buf,
				descriptor_length);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		break;
	default:
		memcpy(data_buf, data, descriptor_length);
		break;
	}

	end_code= STAT_SUCCESS;
end:
	LOGD("Exiting '%s'\n", __func__);
	return end_code;
}

static int psi_desc_enc_dvb_service(log_ctx_t *log_ctx,
		const psi_desc_dvb_service_ctx_t *psi_desc_dvb_service_ctx,
		void *buf, uint8_t descriptor_length)
{
	uint8_t *data_buf;
	uint8_t service_provider_name_length;
	uint8_t service_name_length;
	int size_diff;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(psi_desc_dvb_service_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(buf!= NULL, return STAT_ERROR);

	data_buf= (uint8_t*)buf;
	*data_buf++= psi_desc_dvb_service_ctx->service_type;
	*data_buf++= service_provider_name_length=
			psi_desc_dvb_service_ctx->service_provider_name_length;
	memcpy(data_buf, psi_desc_dvb_service_ctx->service_provider_name,
			service_provider_name_length);
	data_buf+= service_provider_name_length;
	*data_buf++= service_name_length=
			psi_desc_dvb_service_ctx->service_name_length;
	memcpy(data_buf, psi_desc_dvb_service_ctx->service_name,
			service_name_length);
	data_buf+= service_name_length;
	size_diff= data_buf- (uint8_t*)buf;
	if(size_diff!= (int)descriptor_length) {
		LOGEV("Illegal DVB service descriptor size: %d bytes parsed "
				"but %u were declared.\n", size_diff, descriptor_length);
		return STAT_ERROR;
	}
	return STAT_SUCCESS;
}

static int psi_desc_enc_dvb_subt(log_ctx_t *log_ctx,
		const psi_desc_dvb_subt_ctx_t *psi_desc_dvb_subt_ctx, void *buf,
		uint8_t descriptor_length)
{
	const llist_t *psi_desc_dvb_subt_nth_ctx_llist;
	uint8_t *data_buf;
	int _N, i, size_diff;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(psi_desc_dvb_subt_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(buf!= NULL, return STAT_ERROR);

	psi_desc_dvb_subt_nth_ctx_llist=
			psi_desc_dvb_subt_ctx->psi_desc_dvb_subt_nth_ctx_llist;
	CHECK_DO(psi_desc_dvb_subt_nth_ctx_llist!= NULL, return STAT_ERROR);

	data_buf= (uint8_t*)buf;

	/* Check compliance: descriptor loop size. */
	if((descriptor_length& 7)!= 0) {
		LOGEV("Illegal DVB subtitling descriptor: descriptor size "
				"should be multiple of 8 (size of %u is declared).\n",
				descriptor_length);
		return STAT_ERROR;
	}

	/* Parse DVB subtitling descriptors data loop. */
	_N= descriptor_length>> 3;
	CHECK_DO(llist_len(psi_desc_dvb_subt_nth_ctx_llist)== _N,
			return STAT_ERROR);
	for(i= 0; i< _N; i++) {
		register uint32_t _639_language_code;
		register uint16_t composition_page_id;
		register uint16_t ancillary_page_id;
		psi_desc_dvb_subt_nth_ctx_t *psi_desc_dvb_subt_nth_ctx=
				(psi_desc_dvb_subt_nth_ctx_t*)
				llist_get_nth(psi_desc_dvb_subt_nth_ctx_llist, i);
		CHECK_DO(psi_desc_dvb_subt_nth_ctx!= NULL, return STAT_ERROR);

		_639_language_code= psi_desc_dvb_subt_nth_ctx->_639_language_code;
		*data_buf++= (_639_language_code>> 16)& 0xFF;
		*data_buf++= (_639_language_code>> 8)& 0xFF;
		*data_buf++= _639_language_code& 0xFF;
		*data_buf++= psi_desc_dvb_subt_nth_ctx->subtitling_type;
		composition_page_id= psi_desc_dvb_subt_nth_ctx->composition_page_id;
		*data_buf++= (composition_page_id>> 8)& 0xFF;
		*data_buf++= composition_page_id& 0xFF;
		ancillary_page_id= psi_desc_dvb_subt_nth_ctx->ancillary_page_id;
		*data_buf++= (ancillary_page_id>> 8)& 0xFF;
		*data_buf++= ancillary_page_id& 0xFF;
	}
	size_diff= data_buf- (uint8_t*)buf;
	if(size_diff!= (int)descriptor_length) {
		LOGEV("Illegal DVB subtitling descriptor size: %d bytes "
				"parsed but %u were declared.\n", size_diff,
				descriptor_length);
		return STAT_ERROR;
	}
	return STAT_SUCCESS;
}
