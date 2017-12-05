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
 * @file psi_desc_dec.c
 * @Author Rafael Antoniello
 */

#include "psi_desc_dec.h"

#include <stdlib.h>
#include <string.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/bitparser.h>
#include <libmediaprocsutils/llist.h>
#include "psi_desc.h"

/* **** Definitions **** */

#define GET_BITS(b) bitparser_get(bitparser_ctx, (b))
#define FLUSH_BITS(b) bitparser_flush(bitparser_ctx, (b))
#define COPY_BYTES(B) bitparser_copy_bytes(bitparser_ctx, (B))

/* **** Prototypes **** */

static psi_desc_dvb_service_ctx_t* psi_desc_dec_dvb_service(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, size_t size);

static psi_desc_dvb_subt_ctx_t* psi_desc_dec_dvb_subt(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid, size_t size);

/* **** Implementations **** */

psi_desc_ctx_t* psi_desc_dec(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid, size_t max_size)
{
	void **data;
	size_t desc_length= 0;
	psi_desc_ctx_t *psi_desc_ctx= NULL;
	int end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments.
	 * Note: Argument 'log_ctx' is allowed to be 'NULL'.
	 */
	CHECK_DO(bitparser_ctx!= NULL, return NULL);
	//CHECK_DO(max_size> 2, return NULL);

	/* Allocate generic descriptor context structure */
	psi_desc_ctx= psi_desc_ctx_allocate();
	CHECK_DO(psi_desc_ctx!= NULL, goto end);

	/* Parse fields */
	psi_desc_ctx->descriptor_tag= GET_BITS(8);
	desc_length= psi_desc_ctx->descriptor_length= GET_BITS(8);
	if((desc_length+ 2)> max_size) {
		LOGEV("Inconsistent descriptor size: the sum of the sizes of "
				"descriptors is greater than the size declared in stream. "
				"PID= %u (0x%0x).\n", pid, pid);
		goto end;
	}
	data= &psi_desc_ctx->data;
	//LOGV("Found descriptor; tag is: '0x%0x'\n",
	//		psi_desc_ctx->descriptor_tag); //comment-me
	switch(psi_desc_ctx->descriptor_tag) {
	case PSI_DESC_TAG_DVB_SERVICE:
		*data= (void*)psi_desc_dec_dvb_service(log_ctx, bitparser_ctx,
				desc_length);
		break;
	case PSI_DESC_TAG_DVB_SUBT:
		*data= (void*)psi_desc_dec_dvb_subt(log_ctx, bitparser_ctx, pid,
				desc_length);
		break;
	default:
		//LOGV("Unknown descriptor type (tag: %u, length: %u)\n",
		//		psi_desc_ctx->descriptor_tag,
		//		desc_length); // comment-me
		if(desc_length> 0)
			*data= COPY_BYTES(desc_length);
		else
			*data= NULL;
		end_code= STAT_ENOTFOUND;
		goto end;
	}
	if(psi_desc_ctx->data== NULL) goto end;

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS && end_code!= STAT_ENOTFOUND)
		psi_desc_ctx_release(&psi_desc_ctx);
	return psi_desc_ctx;
}

static psi_desc_dvb_service_ctx_t* psi_desc_dec_dvb_service(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, size_t size)
{
	int i, end_code= STAT_ERROR;
	psi_desc_dvb_service_ctx_t* psi_desc_dvb_service_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(bitparser_ctx!= NULL, return NULL);

	/* Allocate DVB service descriptor context structure */
	psi_desc_dvb_service_ctx= psi_desc_dvb_service_ctx_allocate();
	CHECK_DO(psi_desc_dvb_service_ctx!= NULL, goto end);

	/* Parse fields */
	psi_desc_dvb_service_ctx->service_type= GET_BITS(8);
	psi_desc_dvb_service_ctx->service_provider_name_length= GET_BITS(8);
	for(i= 0; i< psi_desc_dvb_service_ctx->service_provider_name_length; i++) {
		psi_desc_dvb_service_ctx->service_provider_name[i]= GET_BITS(8);
	}
	//LOGV("service_provider_name: '%s'\n",
	//		psi_desc_dvb_service_ctx->service_provider_name); // comment-me
	psi_desc_dvb_service_ctx->service_name_length= GET_BITS(8);
	for(i= 0; i< psi_desc_dvb_service_ctx->service_name_length; i++) {
		psi_desc_dvb_service_ctx->service_name[i]= GET_BITS(8);
	}
	//LOGV("service_name: '%s'\n",
	//		psi_desc_dvb_service_ctx->service_name); // comment-me

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		psi_desc_dvb_service_ctx_release(&psi_desc_dvb_service_ctx);
	return psi_desc_dvb_service_ctx;
}

static psi_desc_dvb_subt_ctx_t* psi_desc_dec_dvb_subt(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid, size_t size)
{
	int i, ret_code, end_code= STAT_ERROR;
	psi_desc_dvb_subt_nth_ctx_t *psi_desc_dvb_subt_nth_ctx= NULL;
	psi_desc_dvb_subt_ctx_t* psi_desc_dvb_subt_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(bitparser_ctx!= NULL, return NULL);

	/* Allocate DVB subtitling descriptor context structure */
	psi_desc_dvb_subt_ctx= psi_desc_dvb_subt_ctx_allocate();
	CHECK_DO(psi_desc_dvb_subt_ctx!= NULL, goto end);

	/* Check compliance: descriptor loop size. */
	if((size& 7)!= 0) {
		LOGEV("Illegal DVB subtitling descriptor: descriptor size "
				"should be multiple of 8 bytes (declared size is %d). "
				"PID= %u (0x%0x).\n", (int)size, pid, pid);
		goto end;
	}

	/* Parse DVB subtitling descriptors data loop. */
	for(i= 0; i< (size>> 3); i++) {
		/* Allocate DVB Subtitling descriptor data context structure */
		psi_desc_dvb_subt_nth_ctx= psi_desc_dvb_subt_nth_ctx_allocate();
		CHECK_DO(psi_desc_dvb_subt_nth_ctx!= NULL, goto end);

		/* Parse fields */
		psi_desc_dvb_subt_nth_ctx->_639_language_code= GET_BITS(24);
		//LOGV("DVB subtitle found; language code is: '%s'\n", (char*)
		//		&psi_desc_dvb_subt_nth_ctx->_639_language_code); // comment-me
		psi_desc_dvb_subt_nth_ctx->subtitling_type= GET_BITS(8);
		psi_desc_dvb_subt_nth_ctx->composition_page_id= GET_BITS(16);
		psi_desc_dvb_subt_nth_ctx->ancillary_page_id= GET_BITS(16);

		/* Insert node in list of subtitling descriptor data. */
		ret_code= llist_insert_nth(
				&psi_desc_dvb_subt_ctx->psi_desc_dvb_subt_nth_ctx_llist, i,
				psi_desc_dvb_subt_nth_ctx);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		psi_desc_dvb_subt_nth_ctx=
				NULL; // avoid freeing at the end of function.
	}

	end_code= STAT_SUCCESS;
end:
	psi_desc_dvb_subt_nth_ctx_release(&psi_desc_dvb_subt_nth_ctx);
	if(end_code!= STAT_SUCCESS)
		psi_desc_dvb_subt_ctx_release(&psi_desc_dvb_subt_ctx);
	return psi_desc_dvb_subt_ctx;
}
