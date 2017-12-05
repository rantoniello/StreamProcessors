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
 * @file psi_table.c
 * @Author Rafael Antoniello
 */

#include "psi_table.h"

#include <stdlib.h>
#include <string.h>

#define LOG_CTX_DEFULT
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/llist.h>
#include "psi.h"
#include "psi_dvb.h"
#include "psi_desc.h"

/* **** Implementations **** */

psi_table_ctx_t* psi_table_ctx_allocate()
{
	psi_table_ctx_t *psi_table_ctx= (psi_table_ctx_t*)calloc(1,
			sizeof(psi_table_ctx_t));
	CHECK_DO(psi_table_ctx!= NULL, return NULL);
	return psi_table_ctx;
}

psi_table_ctx_t* psi_table_ctx_dup(const psi_table_ctx_t *psi_table_ctx_arg)
{
	psi_table_ctx_t* psi_table_ctx= NULL;
	int ret_code, end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(psi_table_ctx_arg!= NULL, return NULL);

	/* Allocate PSI table context structure */
	psi_table_ctx= psi_table_ctx_allocate();
	CHECK_DO(psi_table_ctx!= NULL, goto end);

	/* Duplicate list of pointers to PSI section context structures */
	LLIST_DUPLICATE(&psi_table_ctx->psi_section_ctx_llist,
			psi_table_ctx_arg->psi_section_ctx_llist, psi_section_ctx_dup,
			psi_section_ctx_t, ret_code);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		psi_table_ctx_release(&psi_table_ctx);
	return psi_table_ctx;
}

int psi_table_ctx_cmp(psi_table_ctx_t *psi_table_ctx1,
		psi_table_ctx_t *psi_table_ctx2)
{
	int i, sections_cnt1, sections_cnt2;

	/* Check arguments */
	CHECK_DO(psi_table_ctx1!= NULL, return 1);
	CHECK_DO(psi_table_ctx2!= NULL, return 1);

	/* Compare sections list lengths */
	sections_cnt1= llist_len(psi_table_ctx1->psi_section_ctx_llist);
	sections_cnt2= llist_len(psi_table_ctx2->psi_section_ctx_llist);
	if(sections_cnt1!= sections_cnt2)
		return 1;

	/* Compare section by section */
	for(i= 0;; i++) {
		psi_section_ctx_t *s1, *s2;

		/* Get sections */
		s1= (psi_section_ctx_t*)llist_get_nth(
				psi_table_ctx1->psi_section_ctx_llist, i);
		s2= (psi_section_ctx_t*)llist_get_nth(
				psi_table_ctx2->psi_section_ctx_llist, i);
		if(s1== NULL && s2== NULL)
			break; // end of lists reached.
		if(psi_section_ctx_cmp(s1, s2)!= 0)
			return 1;
	}
	return 0;
}

void psi_table_ctx_release(psi_table_ctx_t **ref_psi_table_ctx)
{
	psi_table_ctx_t *psi_table_ctx;

	if(ref_psi_table_ctx== NULL)
		return;

	if((psi_table_ctx= *ref_psi_table_ctx)!= NULL) {
		if(psi_table_ctx->psi_section_ctx_llist!= NULL) {
			/* Release table sections list */
		    while(psi_table_ctx->psi_section_ctx_llist!= NULL) {
		    	psi_section_ctx_t *psi_section_ctx=
		    			llist_pop(&psi_table_ctx->psi_section_ctx_llist);
		    	psi_section_ctx_release(&psi_section_ctx);
		    }
		}
		free(*ref_psi_table_ctx);
		*ref_psi_table_ctx= NULL;
	}
}

psi_table_pat_ctx_t* psi_table_pat_ctx_allocate()
{
	return (psi_table_pat_ctx_t*)psi_table_ctx_allocate();
}

psi_table_pat_ctx_t* psi_table_pat_ctx_dup(const psi_table_pat_ctx_t*
		psi_table_pat_ctx_arg)
{
	return (psi_table_pat_ctx_t*)psi_table_ctx_dup(
			(const psi_table_ctx_t*)psi_table_pat_ctx_arg);
}

int psi_table_pat_ctx_cmp(const psi_table_pat_ctx_t* psi_table_pat_ctx1,
		const psi_table_pat_ctx_t* psi_table_pat_ctx2)
{
	return psi_table_ctx_cmp((psi_table_ctx_t*)psi_table_pat_ctx1,
			(psi_table_ctx_t*)psi_table_pat_ctx2);
}

void psi_table_pat_ctx_release(psi_table_pat_ctx_t **ref_psi_table_pat_ctx)
{
	psi_table_ctx_release((psi_table_ctx_t**)ref_psi_table_pat_ctx);
}

#define FILTER_ID_PID 0
#define FILTER_ID_PROGR_NUM 1
static inline psi_pas_prog_ctx_t* psi_table_pat_ctx_filter_id(
		const psi_table_pat_ctx_t* psi_table_pat_ctx, uint16_t reference_id,
		const int filter_id)
{
	llist_t *n;

	/* Check arguments */
	CHECK_DO(psi_table_pat_ctx!= NULL, return NULL);

	/* Iterate over PAT and get service specific data for this PID */
	for(n= psi_table_pat_ctx->psi_section_ctx_llist; n!= NULL; n= n->next) {
		llist_t *n2;
		psi_section_ctx_t *psi_section_ctx_nth= NULL;
		psi_pas_ctx_t *psi_pas_ctx= NULL;

		psi_section_ctx_nth= (psi_section_ctx_t*)n->data;
		CHECK_DO(psi_section_ctx_nth!= NULL, continue);
		psi_pas_ctx= (psi_pas_ctx_t*)psi_section_ctx_nth->data;
		CHECK_DO(psi_pas_ctx!= NULL, continue);

		/* Iterate over PAS programs */
		for(n2= psi_pas_ctx->psi_pas_prog_ctx_llist; n2!= NULL; n2= n2->next) {
			psi_pas_prog_ctx_t *psi_pas_prog_ctx_nth;

			psi_pas_prog_ctx_nth= (psi_pas_prog_ctx_t*)n2->data;
			CHECK_DO(psi_pas_prog_ctx_nth!= NULL, continue);

			switch(filter_id) {
			case FILTER_ID_PID:
				if(psi_pas_prog_ctx_nth->reference_pid== reference_id)
					return psi_pas_prog_ctx_nth;
				break;
			case FILTER_ID_PROGR_NUM:
				if(psi_pas_prog_ctx_nth->program_number== reference_id)
					return psi_pas_prog_ctx_nth;
				break;
			default:
				break;
			}
		}
	}
	return NULL;
}

psi_pas_prog_ctx_t* psi_table_pat_ctx_filter_pid(
		const psi_table_pat_ctx_t* psi_table_pat_ctx, uint16_t reference_pid)
{
	return psi_table_pat_ctx_filter_id(psi_table_pat_ctx, reference_pid,
			FILTER_ID_PID);
}

psi_pas_prog_ctx_t* psi_table_pat_ctx_filter_program_num(
		const psi_table_pat_ctx_t* psi_table_pat_ctx, uint16_t program_number)
{
	return psi_table_pat_ctx_filter_id(psi_table_pat_ctx, program_number,
			FILTER_ID_PROGR_NUM);
}
#undef FILTER_ID_PID
#undef FILTER_ID_PROGR_NUM

psi_section_ctx_t* psi_table_pmt_ctx_filter_program_pid(
		const psi_table_pat_ctx_t* psi_table_pat_ctx,
		const psi_table_pmt_ctx_t* psi_table_pmt_ctx, uint16_t reference_pid)
{
	llist_t *n;
	uint16_t program_number;
	psi_pas_prog_ctx_t *psi_pas_prog_ctx;

	/* Check arguments */
	CHECK_DO(psi_table_pat_ctx!= NULL, return NULL);
	CHECK_DO(psi_table_pmt_ctx!= NULL, return NULL);

	/* PID<->program_number relation is given in PAT */
	psi_pas_prog_ctx= psi_table_pat_ctx_filter_pid(psi_table_pat_ctx,
			reference_pid);
	if(psi_pas_prog_ctx== NULL)
		return NULL;

	program_number= psi_pas_prog_ctx->program_number;

	/* Iterate over PMT and get service specific data for this program */
	for(n= psi_table_pmt_ctx->psi_section_ctx_llist; n!= NULL; n= n->next) {
		psi_section_ctx_t *psi_section_ctx_nth= NULL;

		psi_section_ctx_nth= (psi_section_ctx_t*)n->data;
		CHECK_DO(psi_section_ctx_nth!= NULL, continue);

		/* Check if this PMS has the required program description
		 * (Note that for the case of a PMS, 'table_id_extension' is assigned
		 * to 'program_number').
		 */
		if(psi_section_ctx_nth->table_id_extension== program_number)
			return psi_section_ctx_nth;
	}
	return NULL;
}

psi_pms_es_ctx_t* psi_table_pmt_ctx_filter_program_pid_es_pid(
		const psi_table_pat_ctx_t* psi_table_pat_ctx,
		const psi_table_pmt_ctx_t* psi_table_pmt_ctx, uint16_t prog_pid,
		uint16_t es_pid)
{
	psi_section_ctx_t *psi_section_ctx_pms= NULL;

	/* Check arguments */
	CHECK_DO(psi_table_pat_ctx!= NULL, return NULL);
	CHECK_DO(psi_table_pmt_ctx!= NULL, return NULL);

	psi_section_ctx_pms= psi_table_pmt_ctx_filter_program_pid(
			psi_table_pat_ctx, psi_table_pmt_ctx, prog_pid);
	if(psi_section_ctx_pms== NULL)
		return NULL;

	return psi_pms_ctx_filter_es_pid(
			(const psi_pms_ctx_t*)psi_section_ctx_pms->data, es_pid);
}

psi_section_ctx_t* psi_table_pmt_ctx_filter_program_num(
		const psi_table_pmt_ctx_t* psi_table_pmt_ctx, uint16_t program_number)
{
	llist_t *n;

	/* Check arguments */
	CHECK_DO(psi_table_pmt_ctx!= NULL, return NULL);

	/* Iterate over PMT and get service specific data for this program */
	for(n= psi_table_pmt_ctx->psi_section_ctx_llist; n!= NULL; n= n->next) {
		psi_section_ctx_t *psi_section_ctx_nth= NULL;

		psi_section_ctx_nth= (psi_section_ctx_t*)n->data;
		CHECK_DO(psi_section_ctx_nth!= NULL, continue);

		/* Check if this PMS has the required program description
		 * (Note that for the case of a PMS, 'table_id_extension' is assigned
		 * to 'program_number').
		 */
		if(psi_section_ctx_nth->table_id_extension== program_number)
			return psi_section_ctx_nth;
	}
	return NULL;
}

psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx_allocate()
{
	return (psi_table_dvb_sdt_ctx_t*)psi_table_ctx_allocate();
}

psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx_dup(
		const psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx_arg)
{
	return (psi_table_dvb_sdt_ctx_t*)psi_table_ctx_dup(
			(const psi_table_ctx_t*)psi_table_dvb_sdt_ctx_arg);
}

void psi_table_dvb_sdt_ctx_release(
		psi_table_dvb_sdt_ctx_t** ref_psi_table_dvb_sdt_ctx)
{
	psi_table_ctx_release((psi_table_dvb_sdt_ctx_t**)ref_psi_table_dvb_sdt_ctx);
}

psi_dvb_sds_prog_ctx_t* psi_table_dvb_sdt_ctx_filter_program_num(
		const psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx,
		uint16_t program_number)
{
	llist_t *n;

	/* Check arguments */
	CHECK_DO(psi_table_dvb_sdt_ctx!= NULL, return NULL);

	/* Iterate over SDT and get service specific data for this program */
	for(n= psi_table_dvb_sdt_ctx->psi_section_ctx_llist; n!= NULL; n= n->next) {
		psi_section_ctx_t *psi_section_ctx_nth= NULL;
		psi_dvb_sds_ctx_t *psi_dvb_sds_ctx= NULL;
		psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx= NULL;

		psi_section_ctx_nth= (psi_section_ctx_t*)n->data;
		CHECK_DO(psi_section_ctx_nth!= NULL, continue);

		/* Check if this SDS has the required program description */
		psi_dvb_sds_ctx= (psi_dvb_sds_ctx_t*)psi_section_ctx_nth->data;
		psi_dvb_sds_prog_ctx= psi_dvb_sds_ctx_filter_program_num(
				psi_dvb_sds_ctx, program_number);
		if(psi_dvb_sds_prog_ctx!= NULL)
			return psi_dvb_sds_prog_ctx;
	}
	return NULL;
}

const char* psi_table_dvb_sdt_ctx_filter_service_name_by_program_num(
		const psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx,
		uint16_t program_number)
{
	psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx;
	psi_desc_ctx_t *psi_desc_ctx;
	psi_desc_dvb_service_ctx_t *psi_desc_dvb_service_ctx;

	/* Check arguments */
	CHECK_DO(psi_table_dvb_sdt_ctx!= NULL, return NULL);

	psi_dvb_sds_prog_ctx= psi_table_dvb_sdt_ctx_filter_program_num(
			psi_table_dvb_sdt_ctx, program_number);
	if(psi_dvb_sds_prog_ctx== NULL ||
			psi_dvb_sds_prog_ctx->psi_desc_ctx_llist== NULL)
		return NULL;

	psi_desc_ctx= psi_desc_ctx_filter_tag(
			psi_dvb_sds_prog_ctx->psi_desc_ctx_llist, NULL,
			PSI_DESC_TAG_DVB_SERVICE);
	if(psi_desc_ctx== NULL)
		return NULL;

	psi_desc_dvb_service_ctx= psi_desc_ctx->data;
	if(psi_desc_dvb_service_ctx== NULL)
		return NULL;

	return psi_desc_dvb_service_ctx->service_name;
}
