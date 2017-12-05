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
 * @file psi_desc.c
 * @Author Rafael Antoniello
 */

#include "psi_desc.h"

#include <stdlib.h>
#include <string.h>

#include <libcjson/cJSON.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/llist.h>

/* **** Definitions **** */

/* **** Prototypes **** */

/* **** Implementations **** */

psi_desc_ctx_t* psi_desc_ctx_allocate()
{
	return (psi_desc_ctx_t*)calloc(1, sizeof(psi_desc_ctx_t));
}

psi_desc_ctx_t* psi_desc_ctx_dup(const psi_desc_ctx_t* psi_desc_ctx_arg)
{
	psi_desc_ctx_t* psi_desc_ctx= NULL;
	int end_code= STAT_ERROR;
	LOG_CTX_INIT(NULL);

	/* CHeck arguments */
	CHECK_DO(psi_desc_ctx_arg!= NULL, return NULL);

	/* Allocate descriptor context structure */
	psi_desc_ctx= psi_desc_ctx_allocate();
	CHECK_DO(psi_desc_ctx!= NULL, goto end);

	/* Copy simple type data members */
	psi_desc_ctx->descriptor_tag= psi_desc_ctx_arg->descriptor_tag;
	psi_desc_ctx->descriptor_length= psi_desc_ctx_arg->descriptor_length;

	/* Duplicate descriptor specific data fields. */
	if(psi_desc_ctx_arg->data!= NULL) {
		void *data_src= psi_desc_ctx_arg->data;
		void **data_dst= &psi_desc_ctx->data;

		switch(psi_desc_ctx_arg->descriptor_tag) {
		case PSI_DESC_TAG_DVB_SERVICE:
			*data_dst= (void*)psi_desc_dvb_service_ctx_dup(
					(const psi_desc_dvb_service_ctx_t*)data_src);
			break;
		case PSI_DESC_TAG_DVB_SUBT:
			*data_dst= (void*)psi_desc_dvb_subt_ctx_dup(
					(const psi_desc_dvb_subt_ctx_t*)data_src);
			break;
		default:
			//LOGV("Unknown descriptor type\n"); //comment-me
			*data_dst= malloc(psi_desc_ctx_arg->descriptor_length);
			CHECK_DO(*data_dst!= NULL, goto end);
			memcpy(*data_dst, data_src, psi_desc_ctx_arg->descriptor_length);
			break;
		}
		CHECK_DO(psi_desc_ctx->data!= NULL, goto end);
	}

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		psi_desc_ctx_release(&psi_desc_ctx);

	return psi_desc_ctx;
}

void psi_desc_ctx_release(psi_desc_ctx_t **ref_psi_desc_ctx)
{
	psi_desc_ctx_t *psi_desc_ctx;

	if(ref_psi_desc_ctx== NULL)
		return;

	if((psi_desc_ctx= *ref_psi_desc_ctx)!= NULL) {
		/* Release specific descriptor data */
		switch(psi_desc_ctx->descriptor_tag) {
		case PSI_DESC_TAG_DVB_SERVICE: {
			psi_desc_dvb_service_ctx_t *psi_desc_dvb_service_ctx=
					(psi_desc_dvb_service_ctx_t*)psi_desc_ctx->data;
			psi_desc_dvb_service_ctx_release(&psi_desc_dvb_service_ctx);
			break;
		}
		case PSI_DESC_TAG_DVB_SUBT: {
			psi_desc_dvb_subt_ctx_t *psi_desc_dvb_subt_ctx=
					(psi_desc_dvb_subt_ctx_t*)psi_desc_ctx->data;
			psi_desc_dvb_subt_ctx_release(&psi_desc_dvb_subt_ctx);
			break;
		}
		default:
			//LOGV("Unknown descriptor type\n"); //comment-me
			if(psi_desc_ctx->data!= NULL)
				free(psi_desc_ctx->data);
			break;
		}
		psi_desc_ctx->data= NULL;

		free(*ref_psi_desc_ctx);
		*ref_psi_desc_ctx= NULL;
	}
}

psi_desc_ctx_t* psi_desc_ctx_filter_tag(llist_t *psi_desc_ctx_llist,
		int *ret_index, int tag)
{
	int i;
	LOG_CTX_INIT(NULL);

	/* Check arguments.
	 * Argument 'ret_index' is allowed to be NULL.
	 */
	CHECK_DO(psi_desc_ctx_llist!= NULL, return NULL);

	for(i= 0;; i++) {
		psi_desc_ctx_t *psi_desc_ctx= (psi_desc_ctx_t*)llist_get_nth(
				psi_desc_ctx_llist, i);
		if(psi_desc_ctx== NULL)
			break; // end of list reached
		if(psi_desc_ctx->descriptor_tag== (uint8_t)tag) {
			if(ret_index!= NULL)
				*ret_index= i;
			return psi_desc_ctx;
		}
	}
	return NULL;
}

psi_desc_dvb_service_ctx_t* psi_desc_dvb_service_ctx_allocate()
{
	return (psi_desc_dvb_service_ctx_t*)calloc(1,
			sizeof(psi_desc_dvb_service_ctx_t));
}

psi_desc_dvb_service_ctx_t* psi_desc_dvb_service_ctx_dup(
		const psi_desc_dvb_service_ctx_t* psi_desc_dvb_service_ctx_arg)
{
	psi_desc_dvb_service_ctx_t *psi_desc_dvb_service_ctx= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(psi_desc_dvb_service_ctx_arg!= NULL, return NULL);

	/* Allocate DVB service descriptor context structure */
	psi_desc_dvb_service_ctx= psi_desc_dvb_service_ctx_allocate();
	CHECK_DO(psi_desc_dvb_service_ctx!= NULL, return NULL);

	/* Copy simple type data members */
	memcpy(psi_desc_dvb_service_ctx, psi_desc_dvb_service_ctx_arg,
			sizeof(psi_desc_dvb_service_ctx_t));

	return psi_desc_dvb_service_ctx;
}

void psi_desc_dvb_service_ctx_release(
		psi_desc_dvb_service_ctx_t **ref_psi_desc_dvb_service_ctx)
{
	psi_desc_dvb_service_ctx_t *psi_desc_dvb_service_ctx;

	if(ref_psi_desc_dvb_service_ctx== NULL)
		return;

	if((psi_desc_dvb_service_ctx= *ref_psi_desc_dvb_service_ctx)!= NULL) {
		free(*ref_psi_desc_dvb_service_ctx);
		*ref_psi_desc_dvb_service_ctx= NULL;
	}
}

psi_desc_dvb_subt_ctx_t* psi_desc_dvb_subt_ctx_allocate()
{
	return (psi_desc_dvb_subt_ctx_t*)calloc(1, sizeof(psi_desc_dvb_subt_ctx_t));
}

psi_desc_dvb_subt_ctx_t* psi_desc_dvb_subt_ctx_dup(
		const psi_desc_dvb_subt_ctx_t* psi_desc_dvb_subt_ctx_arg)
{
	psi_desc_dvb_subt_ctx_t *psi_desc_dvb_subt_ctx= NULL;
	int ret_code, end_code= STAT_ERROR;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(psi_desc_dvb_subt_ctx_arg!= NULL, return NULL);

	/* Allocate DVB subtitling descriptor context structure */
	psi_desc_dvb_subt_ctx= psi_desc_dvb_subt_ctx_allocate();
	CHECK_DO(psi_desc_dvb_subt_ctx!= NULL, goto end);

	/* Duplicate list of subtitling descriptor data. */
	LLIST_DUPLICATE(
			&psi_desc_dvb_subt_ctx->psi_desc_dvb_subt_nth_ctx_llist,
			psi_desc_dvb_subt_ctx_arg->psi_desc_dvb_subt_nth_ctx_llist,
			psi_desc_dvb_subt_nth_ctx_dup,
			psi_desc_dvb_subt_nth_ctx_t, ret_code);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		psi_desc_dvb_subt_ctx_release(&psi_desc_dvb_subt_ctx);
	return psi_desc_dvb_subt_ctx;
}

void psi_desc_dvb_subt_ctx_release(
		psi_desc_dvb_subt_ctx_t **ref_psi_desc_dvb_subt_ctx)
{
	psi_desc_dvb_subt_ctx_t *psi_desc_dvb_subt_ctx;

	if(ref_psi_desc_dvb_subt_ctx== NULL)
		return;

	if((psi_desc_dvb_subt_ctx= *ref_psi_desc_dvb_subt_ctx)!= NULL) {
		/* Release list of subtitling descriptor data. */
	    while(psi_desc_dvb_subt_ctx->psi_desc_dvb_subt_nth_ctx_llist!= NULL) {
	    	psi_desc_dvb_subt_nth_ctx_t *psi_desc_dvb_subt_nth_ctx= llist_pop(
	    			&psi_desc_dvb_subt_ctx->psi_desc_dvb_subt_nth_ctx_llist);
	    	psi_desc_dvb_subt_nth_ctx_release(&psi_desc_dvb_subt_nth_ctx);
	    }
		free(psi_desc_dvb_subt_ctx);
		*ref_psi_desc_dvb_subt_ctx= NULL;
	}
}

int psi_desc_dvb_subt_rest_get(const psi_desc_ctx_t *psi_desc_ctx_subt,
		cJSON **ref_cjson_subt_descript, log_ctx_t *log_ctx)
{
	llist_t *n;
	int ret_code, end_code= STAT_ERROR;
	psi_desc_dvb_subt_ctx_t *psi_desc_dvb_subt_ctx= NULL; // Do not release
	cJSON *cjson_subt_descript= NULL;
	cJSON *cjson_subt_descript_service= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(psi_desc_ctx_subt!= NULL, return STAT_ERROR);
	CHECK_DO(ref_cjson_subt_descript!= NULL, return STAT_ERROR);

	*ref_cjson_subt_descript= NULL;

	/* Some sanity check */
	if(psi_desc_ctx_subt->descriptor_tag!= PSI_DESC_TAG_DVB_SUBT){
		LOGE("Wrong descriptor TAG for DVB-subtitling\n");
		goto end;
	}

	psi_desc_dvb_subt_ctx= (psi_desc_dvb_subt_ctx_t*)psi_desc_ctx_subt->data;
	CHECK_DO(psi_desc_dvb_subt_ctx!= NULL, goto end);

	/* JSON structure for DVB-subtitling descriptor is as follows:
	 * [
	 *     [number,number,number,number],
	 *     [number,number,number,number],
	 *     ...
	 * ]
	 */

	/* Create descriptor REST */
	cjson_subt_descript= cJSON_CreateArray();
	CHECK_DO(cjson_subt_descript!= NULL, goto end);

	/* **** Update descriptor REST **** */
	for(n= psi_desc_dvb_subt_ctx->psi_desc_dvb_subt_nth_ctx_llist; n!= NULL;
			n= n->next) {
		psi_desc_dvb_subt_nth_ctx_t *psi_desc_dvb_subt_nth_ctx= NULL;

		/* Get descriptor service context structure */
		psi_desc_dvb_subt_nth_ctx= (psi_desc_dvb_subt_nth_ctx_t*)n->data;
		CHECK_DO(psi_desc_dvb_subt_nth_ctx!= NULL, continue);

		/* Get service REST item */
		if(cjson_subt_descript_service!= NULL)
			cJSON_Delete(cjson_subt_descript_service);
		ret_code= psi_desc_dvb_subt_nth_rest_get(psi_desc_dvb_subt_nth_ctx,
				&cjson_subt_descript_service, LOG_CTX_GET());
		CHECK_DO(ret_code== STAT_SUCCESS, continue);

		/* Add service REST item into DVB-subtitling REST array */
		cJSON_AddItemToArray(cjson_subt_descript, cjson_subt_descript_service);
		cjson_subt_descript_service= NULL; // Avoid double referencing
	}

	*ref_cjson_subt_descript= cjson_subt_descript;
	cjson_subt_descript= NULL; // Avoid double referencing
	end_code= STAT_SUCCESS;
end:
	if(cjson_subt_descript!= NULL)
		cJSON_Delete(cjson_subt_descript);
	if(cjson_subt_descript_service!= NULL)
		cJSON_Delete(cjson_subt_descript_service);
	return end_code;
}

int psi_desc_dvb_subt_rest_put(const cJSON *cjson_subt_descript,
		psi_desc_ctx_t **ref_psi_desc_ctx_subt, log_ctx_t *log_ctx)
{
	int i, ret_code, end_code= STAT_ERROR, num_desc_services= 0;
	psi_desc_ctx_t *psi_desc_ctx_subt= NULL;
	psi_desc_dvb_subt_ctx_t *psi_desc_dvb_subt_ctx= NULL; // Do not release
	psi_desc_dvb_subt_nth_ctx_t *psi_desc_dvb_subt_nth_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(cjson_subt_descript!= NULL, return STAT_ERROR);
	CHECK_DO(ref_psi_desc_ctx_subt!= NULL, return STAT_ERROR);

	*ref_psi_desc_ctx_subt= NULL;

	psi_desc_ctx_subt= psi_desc_ctx_allocate();
	CHECK_DO(psi_desc_ctx_subt!= NULL, goto end);

	psi_desc_ctx_subt->descriptor_tag= PSI_DESC_TAG_DVB_SUBT;
	psi_desc_ctx_subt->descriptor_length= 0; // change below
	psi_desc_ctx_subt->data= (psi_desc_dvb_subt_ctx_t*)
			psi_desc_dvb_subt_ctx_allocate();
	CHECK_DO(psi_desc_ctx_subt->data!= NULL, goto end);
	psi_desc_dvb_subt_ctx= (psi_desc_dvb_subt_ctx_t*)psi_desc_ctx_subt->data;

	/* JSON structure for DVB-subtitling descriptor is as follows:
	 * [
	 *     [number,number,number,number],
	 *     [number,number,number,number],
	 *     ...
	 * ]
	 */

	num_desc_services= cJSON_GetArraySize((cJSON*)cjson_subt_descript);
	ASSERT(num_desc_services>= 0);

	for(i= 0; i< num_desc_services; i++) {
		cJSON *cjson_subt_descript_service;

		/* Get service REST */
		cjson_subt_descript_service= cJSON_GetArrayItem(
				(cJSON*)cjson_subt_descript, i);
		CHECK_DO(cjson_subt_descript_service!= NULL, continue);

		/* Put service context structure */
		if(psi_desc_dvb_subt_nth_ctx!= NULL)
			psi_desc_dvb_subt_nth_ctx_release(&psi_desc_dvb_subt_nth_ctx);
		ret_code= psi_desc_dvb_subt_nth_rest_put(cjson_subt_descript_service,
				&psi_desc_dvb_subt_nth_ctx, LOG_CTX_GET());
		CHECK_DO(ret_code== STAT_SUCCESS, continue);

		/* Push service context into DVB-subtitling context */
		ret_code= llist_push(
				&psi_desc_dvb_subt_ctx->psi_desc_dvb_subt_nth_ctx_llist,
				psi_desc_dvb_subt_nth_ctx);
		CHECK_DO(ret_code== STAT_SUCCESS, continue);
		psi_desc_dvb_subt_nth_ctx=
				NULL; // Push succeed, avoid double referencing
		psi_desc_ctx_subt->descriptor_length+= 8; // Update descriptor length
	}

	*ref_psi_desc_ctx_subt= psi_desc_ctx_subt;
	psi_desc_ctx_subt= NULL; // Avoid double referencing
	end_code= STAT_SUCCESS;
end:
	if(psi_desc_ctx_subt!= NULL)
		psi_desc_ctx_release(&psi_desc_ctx_subt);
	if(psi_desc_dvb_subt_nth_ctx!= NULL)
		psi_desc_dvb_subt_nth_ctx_release(&psi_desc_dvb_subt_nth_ctx);
	return end_code;
}

psi_desc_dvb_subt_nth_ctx_t* psi_desc_dvb_subt_nth_ctx_allocate()
{
	return (psi_desc_dvb_subt_nth_ctx_t*)calloc(1,
			sizeof(psi_desc_dvb_subt_nth_ctx_t));
}

psi_desc_dvb_subt_nth_ctx_t* psi_desc_dvb_subt_nth_ctx_dup(
		const psi_desc_dvb_subt_nth_ctx_t* psi_desc_dvb_subt_nth_ctx_arg)
{
	psi_desc_dvb_subt_nth_ctx_t *psi_desc_dvb_subt_nth_ctx= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(psi_desc_dvb_subt_nth_ctx_arg!= NULL, return NULL);

	/* Allocate DVB service descriptor context structure */
	psi_desc_dvb_subt_nth_ctx= psi_desc_dvb_subt_nth_ctx_allocate();
	CHECK_DO(psi_desc_dvb_subt_nth_ctx!= NULL, return NULL);

	/* Copy simple type data members */
	memcpy(psi_desc_dvb_subt_nth_ctx, psi_desc_dvb_subt_nth_ctx_arg,
			sizeof(psi_desc_dvb_subt_nth_ctx_t));

	return psi_desc_dvb_subt_nth_ctx;
}

void psi_desc_dvb_subt_nth_ctx_release(
		psi_desc_dvb_subt_nth_ctx_t **ref_psi_desc_dvb_subt_nth_ctx)
{
	psi_desc_dvb_subt_nth_ctx_t *psi_desc_dvb_subt_nth_ctx;

	if(ref_psi_desc_dvb_subt_nth_ctx== NULL)
		return;

	if((psi_desc_dvb_subt_nth_ctx= *ref_psi_desc_dvb_subt_nth_ctx)!= NULL) {
		free(psi_desc_dvb_subt_nth_ctx);
		*ref_psi_desc_dvb_subt_nth_ctx= NULL;
	}
}

int psi_desc_dvb_subt_nth_rest_get(
		const psi_desc_dvb_subt_nth_ctx_t* psi_desc_dvb_subt_nth_ctx,
		cJSON **ref_cjson_descript_service, log_ctx_t *log_ctx)
{
	int end_code= STAT_ERROR;
	cJSON *cjson_descript_service= NULL;
	cJSON *cjson_aux= NULL; // Do not release
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(psi_desc_dvb_subt_nth_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(ref_cjson_descript_service!= NULL, return STAT_ERROR);

	*ref_cjson_descript_service= NULL;

	/* Create descriptor service REST */
	cjson_descript_service= cJSON_CreateArray();
	CHECK_DO(cjson_descript_service!= NULL, goto end);

	/* **** Update service REST **** */

	/* Field '_639_language_code' */
	cjson_aux= cJSON_CreateNumber((double)
			psi_desc_dvb_subt_nth_ctx->_639_language_code);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToArray(cjson_descript_service, cjson_aux);

	/* Field 'subtitling_type' */
	cjson_aux= cJSON_CreateNumber((double)
			psi_desc_dvb_subt_nth_ctx->subtitling_type);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToArray(cjson_descript_service, cjson_aux);

	/* Field 'composition_page_id' */
	cjson_aux= cJSON_CreateNumber((double)
			psi_desc_dvb_subt_nth_ctx->composition_page_id);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToArray(cjson_descript_service, cjson_aux);

	/* Field 'ancillary_page_id' */
	cjson_aux= cJSON_CreateNumber((double)
			psi_desc_dvb_subt_nth_ctx->ancillary_page_id);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToArray(cjson_descript_service, cjson_aux);

	*ref_cjson_descript_service= cjson_descript_service;
	cjson_descript_service= NULL; // Avoid double referencing
	end_code= STAT_SUCCESS;
end:
	if(cjson_descript_service!= NULL)
		cJSON_Delete(cjson_descript_service);
	return end_code;
}

int psi_desc_dvb_subt_nth_rest_put(const cJSON *cjson_descript_service,
		psi_desc_dvb_subt_nth_ctx_t **ref_psi_desc_dvb_subt_nth_ctx,
		log_ctx_t *log_ctx)
{
	int end_code= STAT_ERROR;
	psi_desc_dvb_subt_nth_ctx_t *psi_desc_dvb_subt_nth_ctx= NULL;
	cJSON *cjson_aux= NULL; // Do not release
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(cjson_descript_service!= NULL, return STAT_ERROR);
	CHECK_DO(ref_psi_desc_dvb_subt_nth_ctx!= NULL, return STAT_ERROR);

	*ref_psi_desc_dvb_subt_nth_ctx= NULL;

	/* Allocate service context structure */
	psi_desc_dvb_subt_nth_ctx= psi_desc_dvb_subt_nth_ctx_allocate();
	CHECK_DO(psi_desc_dvb_subt_nth_ctx!= NULL, goto end);

	/* **** Update service context structure **** */

	/* Field '_639_language_code' */
	cjson_aux= cJSON_GetArrayItem((cJSON*)cjson_descript_service, 0);
	CHECK_DO(cjson_aux!= NULL, goto end);
	psi_desc_dvb_subt_nth_ctx->_639_language_code= cjson_aux->valuedouble;
	//LOGV("_639_language_code= %d", (int)cjson_aux->valuedouble); //comment-me

	/* Field 'subtitling_type' */
	cjson_aux= cJSON_GetArrayItem((cJSON*)cjson_descript_service, 1);
	CHECK_DO(cjson_aux!= NULL, goto end);
	psi_desc_dvb_subt_nth_ctx->subtitling_type= cjson_aux->valuedouble;
	//LOGV("subtitling_type= %d", (int)cjson_aux->valuedouble); //comment-me

	/* Field 'composition_page_id' */
	cjson_aux= cJSON_GetArrayItem((cJSON*)cjson_descript_service, 2);
	CHECK_DO(cjson_aux!= NULL, goto end);
	psi_desc_dvb_subt_nth_ctx->composition_page_id= cjson_aux->valuedouble;
	//LOGV("composition_page_id= %d", (int)cjson_aux->valuedouble); //comment-me

	/* Field 'ancillary_page_id' */
	cjson_aux= cJSON_GetArrayItem((cJSON*)cjson_descript_service, 3);
	CHECK_DO(cjson_aux!= NULL, goto end);
	psi_desc_dvb_subt_nth_ctx->ancillary_page_id= cjson_aux->valuedouble;
	//LOGV("ancillary_page_id= %d", (int)cjson_aux->valuedouble); //comment-me

	*ref_psi_desc_dvb_subt_nth_ctx= psi_desc_dvb_subt_nth_ctx;
	psi_desc_dvb_subt_nth_ctx= NULL; // Avoid double referencing
	end_code= STAT_SUCCESS;
end:
	if(psi_desc_dvb_subt_nth_ctx!= NULL)
		psi_desc_dvb_subt_nth_ctx_release(&psi_desc_dvb_subt_nth_ctx);
	return end_code;
}

const const psi_desc_lu_ctx_t psi_desc_lutable[256]=
{
		// Rec. ITU-T H.222.0 | ISO/IEC 13818-1 descriptors
		{0x00, "Reserved"},
		{0x01, "Forbidden"},
		{0x02, "video_stream_descriptor"},
		{0x03, "audio_stream_descriptor"},
		{0x04, "hierarchy_descriptor"},
		{0x05, "registration_descriptor"},
		{0x06, "data_stream_alignment_descriptor"},
		{0x07, "target_background_grid_descriptor"},
		{0x08, "video_window_descriptor"},
		{0x09, "CA_descriptor"},
		{0x0A, "ISO_639_language_descriptor"},
		{0x0B, "system_clock_descriptor"},
		{0x0C, "multiplex_buffer_utilization_descriptor"},
		{0x0D, "copyright_descriptor"},
		{0x0E, "maximum_bitrate_descriptor"},
		{0x0F, "private_data_indicator_descriptor"},
		{0x10, "smoothing_buffer_descriptor"},
		{0x11, "STD_descriptor"},
		{0x12, "IBP_descriptor"},
		{0x13, "Defined_in_ISO/IEC_13818-6"},
		{0x14, "Defined_in_ISO/IEC_13818-6"},
		{0x15, "Defined_in_ISO/IEC_13818-6"},
		{0x16, "Defined_in_ISO/IEC_13818-6"},
		{0x17, "Defined_in_ISO/IEC_13818-6"},
		{0x18, "Defined_in_ISO/IEC_13818-6"},
		{0x19, "Defined_in_ISO/IEC_13818-6"},
		{0x1a, "Defined_in_ISO/IEC_13818-6"},
		{0x1B, "MPEG-4_video_descriptor"},
		{0x1C, "MPEG-4_audio_descriptor"},
		{0x1D, "IOD_descriptor"},
		{0x1E, "SL_descriptor"},
		{0x1F, "FMC_descriptor"},
		{0x20, "external_ES_ID_descriptor"},
		{0x21, "MuxCode_descriptor"},
		{0x22, "FmxBufferSize_descriptor"},
		{0x23, "multiplexBuffer_descriptor"},
		{0x24, "content_labeling_descriptor"},
		{0x25, "metadata_pointer_descriptor"},
		{0x26, "metadata_descriptor"},
		{0x27, "metadata_STD_descriptor"},
		{0x28, "AVC video descriptor"},
		{0x29, "IPMP_descriptor"},
		{0x2A, "AVC_timing_and_HRD_descriptor"},
		{0x2B, "MPEG-2_AAC_audio_descriptor"},
		{0x2C, "FlexMuxTiming_descriptor"},
		{0x2D, "MPEG-4_text_descriptor"},
		{0x2E, "MPEG-4_audio_extension_descriptor"},
		{0x2F, "Auxiliary_video_stream_descriptor"},
		{0x30, "SVC extension descriptor"},
		{0x31, "MVC extension descriptor"},
		{0x32, "J2K video descriptor"},
		{0x33, "MVC operation point descriptor"},
		{0x34, "MPEG2_stereoscopic_video_format_descriptor"},
		{0x35, "Stereoscopic_program_info_descriptor"},
		{0x36, "Stereoscopic_video_info_descriptor"},
		{0x37, "Transport_profile_descriptor"},
		{0x38, "HEVC video descriptor"},
		{0x39, "Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved"},
		{0x3a, "Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved"},
		{0x3b, "Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved"},
		{0x3c, "Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved"},
		{0x3d, "Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved"},
		{0x3e, "Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved"},
		{0x3F, "Extension_descriptor"},
		// DVB descriptors (ETSI EN 300 468)
		{0x40, "network_name_descriptor"},
		{0x41, "service_list_descriptor"},
		{0x42, "stuffing_descriptor"},
		{0x43, "satellite_delivery_system_descriptor"},
		{0x44, "cable_delivery_system_descriptor"},
		{0x45, "VBI_data_descriptor"},
		{0x46, "VBI_teletext_descriptor"},
		{0x47, "bouquet_name_descriptor"},
		{0x48, "service_descriptor"},
		{0x49, "country_availability_descriptor"},
		{0x4A, "linkage_descriptor"},
		{0x4B, "NVOD_reference_descriptor"},
		{0x4C, "time_shifted_service_descriptor"},
		{0x4D, "short_event_descriptor"},
		{0x4E, "extended_event_descriptor"},
		{0x4F, "time_shifted_event_descriptor"},
		{0x50, "component_descriptor"},
		{0x51, "mosaic_descriptor"},
		{0x52, "stream_identifier_descriptor"},
		{0x53, "CA_identifier_descriptor"},
		{0x54, "content_descriptor"},
		{0x55, "parental_rating_descriptor"},
		{0x56, "teletext_descriptor"},
		{0x57, "telephone_descriptor"},
		{0x58, "local_time_offset_descriptor"},
		{0x59, "subtitling_descriptor"},
		{0x5A, "terrestrial_delivery_system_descriptor"},
		{0x5B, "multilingual_network_name_descriptor"},
		{0x5C, "multilingual_bouquet_name_descriptor"},
		{0x5D, "multilingual_service_name_descriptor"},
		{0x5E, "multilingual_component_descriptor"},
		{0x5F, "private_data_specifier_descriptor"},
		{0x60, "service_move_descriptor"},
		{0x61, "short_smoothing_buffer_descriptor"},
		{0x62, "frequency_list_descriptor"},
		{0x63, "partial_transport_stream_descriptor"},
		{0x64, "data_broadcast_descriptor"},
		{0x65, "scrambling_descriptor"},
		{0x66, "data_broadcast_id_descriptor"},
		{0x67, "transport_stream_descriptor"},
		{0x68, "DSNG_descriptor"},
		{0x69, "PDC_descriptor"},
		{0x6A, "AC-3_descriptor"},
		{0x6B, "ancillary_data_descriptor"},
		{0x6C, "cell_list_descriptor"},
		{0x6D, "cell_frequency_link_descriptor"},
		{0x6E, "announcement_support_descriptor"},
		{0x6F, "application_signalling_descriptor"},
		{0x70, "adaptation_field_data_descriptor"},
		{0x71, "service_identifier_descriptor"},
		{0x72, "service_availability_descriptor"},
		{0x73, "default_authority_descriptor"},
		{0x74, "related_content_descriptor"},
		{0x75, "TVA_id_descriptor"},
		{0x76, "content_identifier_descriptor"},
		{0x77, "time_slice_fec_identifier_descriptor"},
		{0x78, "ECM_repetition_rate_descriptor"},
		{0x79, "S2_satellite_delivery_system_descriptor"},
		{0x7A, "enhanced_AC-3_descriptor"},
		{0x7B, "DTS_descriptor"},
		{0x7C, "AAC_descriptor"},
		{0x7D, "XAIT_location_descriptor"},
		{0x7E, "FTA_content_management_descriptor"},
		{0x7F, "extension descriptor"},
		{0x80, "user_defined"},
		{0x81, "user_defined"},
		{0x82, "user_defined"},
		{0x83, "user_defined"},
		{0x84, "user_defined"},
		{0x85, "user_defined"},
		{0x86, "user_defined"},
		{0x87, "user_defined"},
		{0x88, "user_defined"},
		{0x89, "user_defined"},
		{0x8a, "user_defined"},
		{0x8b, "user_defined"},
		{0x8c, "user_defined"},
		{0x8d, "user_defined"},
		{0x8e, "user_defined"},
		{0x8f, "user_defined"},
		{0x90, "user_defined"},
		{0x91, "user_defined"},
		{0x92, "user_defined"},
		{0x93, "user_defined"},
		{0x94, "user_defined"},
		{0x95, "user_defined"},
		{0x96, "user_defined"},
		{0x97, "user_defined"},
		{0x98, "user_defined"},
		{0x99, "user_defined"},
		{0x9a, "user_defined"},
		{0x9b, "user_defined"},
		{0x9c, "user_defined"},
		{0x9d, "user_defined"},
		{0x9e, "user_defined"},
		{0x9f, "user_defined"},
		{0xa0, "user_defined"},
		{0xa1, "user_defined"},
		{0xa2, "user_defined"},
		{0xa3, "user_defined"},
		{0xa4, "user_defined"},
		{0xa5, "user_defined"},
		{0xa6, "user_defined"},
		{0xa7, "user_defined"},
		{0xa8, "user_defined"},
		{0xa9, "user_defined"},
		{0xaa, "user_defined"},
		{0xab, "user_defined"},
		{0xac, "user_defined"},
		{0xad, "user_defined"},
		{0xae, "user_defined"},
		{0xaf, "user_defined"},
		{0xb0, "user_defined"},
		{0xb1, "user_defined"},
		{0xb2, "user_defined"},
		{0xb3, "user_defined"},
		{0xb4, "user_defined"},
		{0xb5, "user_defined"},
		{0xb6, "user_defined"},
		{0xb7, "user_defined"},
		{0xb8, "user_defined"},
		{0xb9, "user_defined"},
		{0xba, "user_defined"},
		{0xbb, "user_defined"},
		{0xbc, "user_defined"},
		{0xbd, "user_defined"},
		{0xbe, "user_defined"},
		{0xbf, "user_defined"},
		{0xc0, "user_defined"},
		{0xc1, "user_defined"},
		{0xc2, "user_defined"},
		{0xc3, "user_defined"},
		{0xc4, "user_defined"},
		{0xc5, "user_defined"},
		{0xc6, "user_defined"},
		{0xc7, "user_defined"},
		{0xc8, "user_defined"},
		{0xc9, "user_defined"},
		{0xca, "user_defined"},
		{0xcb, "user_defined"},
		{0xcc, "user_defined"},
		{0xcd, "user_defined"},
		{0xce, "user_defined"},
		{0xcf, "user_defined"},
		{0xd0, "user_defined"},
		{0xd1, "user_defined"},
		{0xd2, "user_defined"},
		{0xd3, "user_defined"},
		{0xd4, "user_defined"},
		{0xd5, "user_defined"},
		{0xd6, "user_defined"},
		{0xd7, "user_defined"},
		{0xd8, "user_defined"},
		{0xd9, "user_defined"},
		{0xda, "user_defined"},
		{0xdb, "user_defined"},
		{0xdc, "user_defined"},
		{0xdd, "user_defined"},
		{0xde, "user_defined"},
		{0xdf, "user_defined"},
		{0xe0, "user_defined"},
		{0xe1, "user_defined"},
		{0xe2, "user_defined"},
		{0xe3, "user_defined"},
		{0xe4, "user_defined"},
		{0xe5, "user_defined"},
		{0xe6, "user_defined"},
		{0xe7, "user_defined"},
		{0xe8, "user_defined"},
		{0xe9, "user_defined"},
		{0xea, "user_defined"},
		{0xeb, "user_defined"},
		{0xec, "user_defined"},
		{0xed, "user_defined"},
		{0xee, "user_defined"},
		{0xef, "user_defined"},
		{0xf0, "user_defined"},
		{0xf1, "user_defined"},
		{0xf2, "user_defined"},
		{0xf3, "user_defined"},
		{0xf4, "user_defined"},
		{0xf5, "user_defined"},
		{0xf6, "user_defined"},
		{0xf7, "user_defined"},
		{0xf8, "user_defined"},
		{0xf9, "user_defined"},
		{0xfa, "user_defined"},
		{0xfb, "user_defined"},
		{0xfc, "user_defined"},
		{0xfd, "user_defined"},
		{0xfe, "user_defined"},
		{0xFF, "forbidden"}
};
