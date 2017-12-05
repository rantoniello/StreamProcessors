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
 * @file psi.c
 * @Author Rafael Antoniello
 */

#include "psi.h"

#include <stdlib.h>
#include <string.h>

#define LOG_CTX_DEFULT
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/llist.h>
#include "psi_desc.h"
#include "psi_dvb.h"
#include "ts.h"

/* **** Definitions **** */

/* **** Implementations **** */

psi_section_ctx_t* psi_section_ctx_allocate()
{
	return (psi_section_ctx_t*)calloc(1, sizeof(psi_section_ctx_t));
}

psi_section_ctx_t* psi_section_ctx_dup(
		const psi_section_ctx_t* psi_section_ctx_arg)
{
	psi_section_ctx_t *psi_section_ctx= NULL;
	int end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(psi_section_ctx_arg!= NULL, return NULL);

	/* Allocate section context structure */
	psi_section_ctx= psi_section_ctx_allocate();
	CHECK_DO(psi_section_ctx!= NULL, goto end);

	/* Copy all structure members at the exception of pointer values */
	memcpy(psi_section_ctx, psi_section_ctx_arg, sizeof(psi_section_ctx_t));
	psi_section_ctx->data= NULL;

	/* Duplicate PSI section specific data */
	if(psi_section_ctx_arg->data!= NULL) {
		void *data_src= psi_section_ctx_arg->data;
		void **data_dst= &psi_section_ctx->data;
		uint8_t table_id= psi_section_ctx_arg->table_id;

		switch(table_id) {
		case PSI_TABLE_PROGRAM_ASSOCIATION_SECTION:
			*data_dst= psi_pas_ctx_dup((const psi_pas_ctx_t*)data_src);
			break;
		case PSI_TABLE_TS_PROGRAM_MAP_SECTION:
			*data_dst= psi_pms_ctx_dup((const psi_pms_ctx_t*)data_src);
			break;
		case PSI_DVB_SERVICE_DESCR_SECTION_ACTUAL:
			*data_dst= psi_dvb_sds_ctx_dup((const psi_dvb_sds_ctx_t*)data_src);
			break;
		default:
			LOGE("Unknown PSI section type (table id: %u)\n", table_id);
			break;
		}
		CHECK_DO(psi_section_ctx->data!= NULL, goto end);
	}

	end_code= STAT_SUCCESS;

end:
	if(end_code!= STAT_SUCCESS)
		psi_section_ctx_release(&psi_section_ctx);

	return psi_section_ctx;
}

int psi_section_ctx_cmp(const psi_section_ctx_t* psi_section_ctx1,
		const psi_section_ctx_t* psi_section_ctx2)
{
	int ret_value= 1; // means "non-equal"

	/* Check arguments */
	CHECK_DO(psi_section_ctx1!= NULL, return 1);
	CHECK_DO(psi_section_ctx2!= NULL, return 1);

	/* Compare representative fields of sections */
	if(psi_section_ctx1->table_id!= psi_section_ctx2->table_id)
		goto end;
	else if(psi_section_ctx1->section_length!= psi_section_ctx2->section_length)
		goto end;
	else if(psi_section_ctx1->table_id_extension!=
			psi_section_ctx2->table_id_extension)
		goto end;
	else if(psi_section_ctx1->version_number!= psi_section_ctx2->version_number)
		goto end;
	else if(psi_section_ctx1->crc_32!= psi_section_ctx2->crc_32)
		goto end;

	// TODO: compare PSI specific data

	ret_value= 0; // sections are equal
end:
	return ret_value;
}

void psi_section_ctx_release(psi_section_ctx_t **ref_psi_section_ctx)
{
	psi_section_ctx_t *psi_section_ctx;

	if(ref_psi_section_ctx== NULL)
		return;

	if((psi_section_ctx= *ref_psi_section_ctx)!= NULL) {
		if(psi_section_ctx->data!= NULL) {
			void **ref_data= &psi_section_ctx->data;
			uint8_t table_id= psi_section_ctx->table_id;

			switch(table_id) {
			case PSI_TABLE_PROGRAM_ASSOCIATION_SECTION:
				psi_pas_ctx_release((psi_pas_ctx_t**)ref_data);
				break;
			case PSI_TABLE_TS_PROGRAM_MAP_SECTION:
				psi_pms_ctx_release((psi_pms_ctx_t**)ref_data);
				break;
			case PSI_DVB_SERVICE_DESCR_SECTION_ACTUAL:
				psi_dvb_sds_ctx_release((psi_dvb_sds_ctx_t**)ref_data);
				break;
			default:
				LOGE("Unknown PSI section type (table id: %u)\n", table_id);
				break;
			}
		}
		free(psi_section_ctx);
		*ref_psi_section_ctx= NULL;
	}
}

void psi_section_ctx_trace(const psi_section_ctx_t *psi_section_ctx)
{
	CHECK_DO(psi_section_ctx!= NULL, return);

	LOG("> ====== PSI Section ======\n");
	LOGV("> Table id.: %u\n", psi_section_ctx->table_id);
	LOGV("> Section syntax ind.: %u\n",
			psi_section_ctx->section_syntax_indicator);
	LOGV("> Section length: %u\n", psi_section_ctx->section_length);
	LOGV("> Table id. extension: %u\n", psi_section_ctx->table_id_extension);
	LOGV("> Version number: %u\n", psi_section_ctx->version_number);
	LOGV("> Current next indicator: %u\n",
			psi_section_ctx->current_next_indicator);
	LOGV("> Section number: %u\n", psi_section_ctx->section_number);
	LOGV("> Last section number: %u\n", psi_section_ctx->last_section_number);
	LOGV("> Specific data: %s\n", psi_section_ctx->data!= NULL? "YES": "N/A");
	if(psi_section_ctx->data!= NULL) {
		void *data= psi_section_ctx->data;

		switch(psi_section_ctx->table_id) {
		case PSI_TABLE_PROGRAM_ASSOCIATION_SECTION:
			psi_pas_ctx_trace((const psi_pas_ctx_t*)data);
			break;
		case PSI_TABLE_TS_PROGRAM_MAP_SECTION:
			psi_pms_ctx_trace((const psi_pms_ctx_t *)data);
			break;
		case PSI_DVB_SERVICE_DESCR_SECTION_ACTUAL:
		{
#if 0 //FIXME
			int i, j;
			psi_dvb_sds_ctx_t *data=
					(psi_dvb_sds_ctx_t*)psi_section_ctx->data;
			LOGV("* --- DVB services loop ---\n");
			for(i= 0;; i++) {
				psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx_t_nth;
				psi_dvb_sds_prog_ctx_t_nth= (psi_dvb_sds_prog_ctx_t*)
						llist_get_nth((const llist_t*)data->services, 0);
				if(psi_dvb_sds_prog_ctx_t_nth== NULL)
					break; // end of list reached
				LOGV("* --- Service[%d]  ---\n", i);
				LOGV("* service_id: %u\n",
						psi_dvb_sds_prog_ctx_t_nth->service_id);
				for(j= 0;; j++) {
					psi_desc_ctx_t *psi_desc_ctx;
					psi_desc_ctx= (psi_desc_ctx_t*)llist_get_nth(
							(const llist_t*)
							psi_dvb_sds_prog_ctx_t_nth->descriptors, j);
					if(psi_desc_ctx== NULL)
						break; // end of list reached
					if(psi_desc_ctx->descriptor_tag!=
							PSI_DESC_TAG_DVB_SERVICE)
						continue;
					LOGV("* service_name: %s\n",
							((psi_desc_dvb_service_ctx_t*)psi_desc_ctx)
							->service_name);
				}
				LOGV("* -------------------\n");
			}
#endif
			break;
		}
		default:
		{
			LOGV("* Not supported/unknown PSI table\n");
			break;
		}
		}
	}
	LOGV("> CRC: 0x%0x\n", psi_section_ctx->crc_32);
	LOGV(">\n");
}

psi_pas_ctx_t* psi_pas_ctx_allocate()
{
	return (psi_pas_ctx_t*)calloc(1, sizeof(psi_pas_ctx_t));
}

psi_pas_ctx_t* psi_pas_ctx_dup(const psi_pas_ctx_t* psi_pas_ctx_arg)
{
	psi_pas_ctx_t *psi_pas_ctx= NULL;
	int ret_code, end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(psi_pas_ctx_arg!= NULL, return NULL);

	/* Allocate PMS context structure */
	psi_pas_ctx= psi_pas_ctx_allocate();
	CHECK_DO(psi_pas_ctx!= NULL, goto end);

	/* Duplicate list of pointers to PAS specific data context structures */
	LLIST_DUPLICATE(&psi_pas_ctx->psi_pas_prog_ctx_llist,
			psi_pas_ctx_arg->psi_pas_prog_ctx_llist, psi_pas_prog_ctx_dup,
			psi_pas_prog_ctx_t, ret_code);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;

end:
	if(end_code!= STAT_SUCCESS)
		psi_pas_ctx_release(&psi_pas_ctx);

	return psi_pas_ctx;
}

void psi_pas_ctx_release(psi_pas_ctx_t **ref_psi_pas_ctx)
{
	psi_pas_ctx_t *psi_pas_ctx;

	if(ref_psi_pas_ctx== NULL)
		return;

	if((psi_pas_ctx= *ref_psi_pas_ctx)!= NULL) {
		/* Release list of pointers to PAS specific data context structures */
	    while(psi_pas_ctx->psi_pas_prog_ctx_llist!= NULL) {
	    	psi_pas_prog_ctx_t *psi_pas_prog_ctx=
	    			llist_pop(&psi_pas_ctx->psi_pas_prog_ctx_llist);
	    	psi_pas_prog_ctx_release(&psi_pas_prog_ctx);
	    }
		free(*ref_psi_pas_ctx);
		*ref_psi_pas_ctx= NULL;
	}
}

void psi_pas_ctx_trace(const psi_pas_ctx_t *psi_pas_ctx)
{
	int i;

	CHECK_DO(psi_pas_ctx!= NULL, return);

	for(i= 0;; i++) {
		psi_pas_prog_ctx_t *psi_pas_prog_ctx= (psi_pas_prog_ctx_t*)
				llist_get_nth(psi_pas_ctx->psi_pas_prog_ctx_llist, i);
		if(psi_pas_prog_ctx== NULL)
			break; // End of list reached
		psi_pas_prog_ctx_trace((const psi_pas_prog_ctx_t*)psi_pas_prog_ctx);
	}
}

psi_pas_prog_ctx_t* psi_pas_prog_ctx_allocate()
{
	return (psi_pas_prog_ctx_t*)calloc(1, sizeof(psi_pas_prog_ctx_t));
}

psi_pas_prog_ctx_t* psi_pas_prog_ctx_dup(
		const psi_pas_prog_ctx_t* psi_pas_prog_ctx_arg)
{
	psi_pas_prog_ctx_t* psi_pas_prog_ctx= psi_pas_prog_ctx_allocate();
	CHECK_DO(psi_pas_prog_ctx!= NULL, return NULL);
	memcpy(psi_pas_prog_ctx, psi_pas_prog_ctx_arg, sizeof(psi_pas_prog_ctx_t));
	return psi_pas_prog_ctx;
}

void psi_pas_prog_ctx_release(psi_pas_prog_ctx_t **ref_psi_pas_prog_ctx)
{
	if(ref_psi_pas_prog_ctx== NULL)
		return;

	if((*ref_psi_pas_prog_ctx)!= NULL) {
		free(*ref_psi_pas_prog_ctx);
		*ref_psi_pas_prog_ctx= NULL;
	}
}

void psi_pas_prog_ctx_trace(const psi_pas_prog_ctx_t *psi_pas_prog_ctx)
{
	CHECK_DO(psi_pas_prog_ctx!= NULL, return);
	LOGV(">> program_number: %u\n", psi_pas_prog_ctx->program_number);
	LOGV(">> reference_pid: %u\n", psi_pas_prog_ctx->reference_pid);
}

psi_pms_ctx_t* psi_pms_ctx_allocate()
{
	return (psi_pms_ctx_t*)calloc(1, sizeof(psi_pms_ctx_t));
}

psi_pms_ctx_t* psi_pms_ctx_dup(const psi_pms_ctx_t* psi_pms_ctx_arg)
{
	psi_pms_ctx_t *psi_pms_ctx= NULL;
	int ret_code, end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(psi_pms_ctx_arg!= NULL, return NULL);

	/* Allocate PMS context structure */
	psi_pms_ctx= psi_pms_ctx_allocate();
	CHECK_DO(psi_pms_ctx!= NULL, goto end);

	/* Copy all structure members at the exception of pointer values */
	psi_pms_ctx->pcr_pid= psi_pms_ctx_arg->pcr_pid;
	psi_pms_ctx->program_info_length= psi_pms_ctx_arg->program_info_length;
	psi_pms_ctx->crc_32= psi_pms_ctx_arg->crc_32;

	/* Duplicate list of program information descriptors. */
	LLIST_DUPLICATE(&psi_pms_ctx->psi_desc_ctx_llist,
			psi_pms_ctx_arg->psi_desc_ctx_llist, psi_desc_ctx_dup,
			psi_desc_ctx_t, ret_code);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Duplicate list of elementary stream information context structures. */
	LLIST_DUPLICATE(&psi_pms_ctx->psi_pms_es_ctx_llist,
			psi_pms_ctx_arg->psi_pms_es_ctx_llist, psi_pms_es_ctx_dup,
			psi_pms_es_ctx_t, ret_code);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;

end:
	if(end_code!= STAT_SUCCESS)
		psi_pms_ctx_release(&psi_pms_ctx);
	return psi_pms_ctx;
}

psi_pms_es_ctx_t* psi_pms_ctx_filter_es_pid(const psi_pms_ctx_t *psi_pms_ctx,
		uint16_t es_pid)
{
	llist_t *n;

	/* Check arguments */
	CHECK_DO(psi_pms_ctx!= NULL, return NULL);
	CHECK_DO(es_pid< TS_MAX_PID_VAL, return NULL);

	/* Iterate over PMS and get elementary stream information */
	for(n= psi_pms_ctx->psi_pms_es_ctx_llist; n!= NULL; n= n->next) {
		psi_pms_es_ctx_t *psi_pms_es_ctx;

		psi_pms_es_ctx= (psi_pms_es_ctx_t*)n->data;
		CHECK_DO(psi_pms_es_ctx!= NULL, continue);

		if(psi_pms_es_ctx->elementary_PID== es_pid)
			return psi_pms_es_ctx;
	}
	return NULL;
}

void psi_pms_ctx_release(psi_pms_ctx_t **ref_psi_pms_ctx)
{
	psi_pms_ctx_t *psi_pms_ctx;

	if(ref_psi_pms_ctx== NULL)
		return;

	if((psi_pms_ctx= *ref_psi_pms_ctx)!= NULL) {
		/* Release list of program information descriptors. */
	    while(psi_pms_ctx->psi_desc_ctx_llist!= NULL) {
	    	psi_desc_ctx_t *psi_desc_ctx=
	    			llist_pop(&psi_pms_ctx->psi_desc_ctx_llist);
	    	psi_desc_ctx_release(&psi_desc_ctx);
	    }
		/* Release list of elementary stream information context structures. */
	    while(psi_pms_ctx->psi_pms_es_ctx_llist!= NULL) {
	    	psi_pms_es_ctx_t *psi_pms_es_ctx=
	    			llist_pop(&psi_pms_ctx->psi_pms_es_ctx_llist);
	    	psi_pms_es_ctx_release(&psi_pms_es_ctx);
	    }
		free(*ref_psi_pms_ctx);
		*ref_psi_pms_ctx= NULL;
	}
}

void psi_pms_ctx_trace(const psi_pms_ctx_t *psi_pms_ctx)
{
	int i;

	CHECK_DO(psi_pms_ctx!= NULL, return);

	LOGV("> PCR_PID: %u\n", psi_pms_ctx->pcr_pid);
	LOGV("> program_info_length: %u\n", psi_pms_ctx->program_info_length);
	for(i= 0;; i++) {
		psi_pms_es_ctx_t *psi_pms_es_ctx= (psi_pms_es_ctx_t*)
				llist_get_nth(psi_pms_ctx->psi_pms_es_ctx_llist, i);
		if(psi_pms_es_ctx== NULL)
			break; // End of list reached
		LOGV(">> '%d°' Elementary Stream data: \n", i);
		psi_pms_es_ctx_trace((const psi_pms_es_ctx_t*)psi_pms_es_ctx);
	}
}

psi_pms_es_ctx_t* psi_pms_es_ctx_allocate()
{
	return (psi_pms_es_ctx_t*)calloc(1, sizeof(psi_pms_es_ctx_t));
}

psi_pms_es_ctx_t* psi_pms_es_ctx_dup(const psi_pms_es_ctx_t* psi_pms_es_ctx_arg)
{
	psi_pms_es_ctx_t *psi_pms_es_ctx= NULL;
	int ret_code, end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(psi_pms_es_ctx_arg!= NULL, NULL);

	/* Allocate program ES context structure */
	psi_pms_es_ctx= psi_pms_es_ctx_allocate();
	CHECK_DO(psi_pms_es_ctx!= NULL, goto end);

	/* Copy all structure members at the exception of pointer values */
	psi_pms_es_ctx->stream_type= psi_pms_es_ctx_arg->stream_type;
	psi_pms_es_ctx->elementary_PID= psi_pms_es_ctx_arg->elementary_PID;
	psi_pms_es_ctx->es_info_length= psi_pms_es_ctx_arg->es_info_length;

	/* Duplicate list of Elementary Stream (ES) information descriptors. */
	LLIST_DUPLICATE(&psi_pms_es_ctx->psi_desc_ctx_llist,
			psi_pms_es_ctx_arg->psi_desc_ctx_llist, psi_desc_ctx_dup,
			psi_desc_ctx_t, ret_code);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;

end:
	if(end_code!= STAT_SUCCESS)
		psi_pms_es_ctx_release(&psi_pms_es_ctx);

	return psi_pms_es_ctx;
}

void psi_pms_es_ctx_release(psi_pms_es_ctx_t **ref_psi_pms_es_ctx)
{
	psi_pms_es_ctx_t *psi_pms_es_ctx;

	if(ref_psi_pms_es_ctx== NULL)
		return;

	if((psi_pms_es_ctx= *ref_psi_pms_es_ctx)!= NULL) {
		/* Release list of Elementary Stream (ES) information descriptors. */
	    while(psi_pms_es_ctx->psi_desc_ctx_llist!= NULL) {
	    	psi_desc_ctx_t *psi_desc_ctx=
	    			llist_pop(&psi_pms_es_ctx->psi_desc_ctx_llist);
	    	psi_desc_ctx_release(&psi_desc_ctx);
	    }
		free(*ref_psi_pms_es_ctx);
		*ref_psi_pms_es_ctx= NULL;
	}
}

void psi_pms_es_ctx_trace(const psi_pms_es_ctx_t *psi_pms_es_ctx)
{
	CHECK_DO(psi_pms_es_ctx!= NULL, return);
	LOGV(">> stream_type: %u\n", psi_pms_es_ctx->stream_type);
	LOGV(">> elementary_PID: %u\n", psi_pms_es_ctx->elementary_PID);
	LOGV(">> ES_info_length: %u\n", psi_pms_es_ctx->es_info_length);
	LOGV(">>\n");
}
