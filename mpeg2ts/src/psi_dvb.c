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
 * @file psi_dvb.c
 * @Author Rafael Antoniello
 */

#include "psi_dvb.h"

#include <stdlib.h>
#include <string.h>

#define LOG_CTX_DEFULT
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/llist.h>
#include "psi_desc.h"

/* **** Definitions **** */

/* **** Prototypes **** */

/* **** Implementations **** */

psi_dvb_sds_ctx_t* psi_dvb_sds_ctx_allocate()
{
	psi_dvb_sds_ctx_t *psi_dvb_sds_ctx= NULL;

	psi_dvb_sds_ctx= (psi_dvb_sds_ctx_t*)calloc(1, sizeof(psi_dvb_sds_ctx_t));
	CHECK_DO(psi_dvb_sds_ctx!= NULL, return NULL);
	return psi_dvb_sds_ctx;
}

psi_dvb_sds_ctx_t* psi_dvb_sds_ctx_dup(
		const psi_dvb_sds_ctx_t* psi_dvb_sds_ctx_arg)
{
	psi_dvb_sds_ctx_t *psi_dvb_sds_ctx= NULL;
	int ret_code, end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(psi_dvb_sds_ctx_arg!= NULL, return NULL);

	/* Allocate SDS specific data context structure */
	psi_dvb_sds_ctx= psi_dvb_sds_ctx_allocate();
	CHECK_DO(psi_dvb_sds_ctx!= NULL, goto end);

	/* Copy simple type data */
	psi_dvb_sds_ctx->original_network_id=
			psi_dvb_sds_ctx_arg->original_network_id;

	/* Duplicate services data list */
	LLIST_DUPLICATE(&psi_dvb_sds_ctx->psi_dvb_sds_prog_ctx_llist,
			psi_dvb_sds_ctx_arg->psi_dvb_sds_prog_ctx_llist,
			psi_dvb_sds_prog_ctx_dup,
			psi_dvb_sds_prog_ctx_t, ret_code);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;

end:
	if(end_code!= STAT_SUCCESS)
		psi_dvb_sds_ctx_release(&psi_dvb_sds_ctx);
	return psi_dvb_sds_ctx;
}

void psi_dvb_sds_ctx_release(psi_dvb_sds_ctx_t **ref_psi_dvb_sds_ctx)
{
	psi_dvb_sds_ctx_t *psi_dvb_sds_ctx;

	if(ref_psi_dvb_sds_ctx== NULL)
		return;

	if((psi_dvb_sds_ctx= *ref_psi_dvb_sds_ctx)!= NULL) {
		/* Release services data list */
	    while(psi_dvb_sds_ctx->psi_dvb_sds_prog_ctx_llist!= NULL) {
	    	psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx=
	    			llist_pop(&psi_dvb_sds_ctx->psi_dvb_sds_prog_ctx_llist);
	    	psi_dvb_sds_prog_ctx_release(&psi_dvb_sds_prog_ctx);
	    }
		free(psi_dvb_sds_ctx);
		*ref_psi_dvb_sds_ctx= NULL;
	}
}

psi_dvb_sds_prog_ctx_t* psi_dvb_sds_ctx_filter_program_num(
		const psi_dvb_sds_ctx_t* psi_dvb_sds_ctx, uint16_t program_number)
{
	llist_t *n;

	/* Check arguments */
	CHECK_DO(psi_dvb_sds_ctx!= NULL, return NULL);

	/* Iterate over services data list */
	for(n= psi_dvb_sds_ctx->psi_dvb_sds_prog_ctx_llist; n!= NULL; n= n->next) {
		psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx_nth;

		psi_dvb_sds_prog_ctx_nth= (psi_dvb_sds_prog_ctx_t*)n->data;
		CHECK_DO(psi_dvb_sds_prog_ctx_nth!= NULL, continue);

		/* Get service information for this program only */
		if(program_number== psi_dvb_sds_prog_ctx_nth->service_id)
			return psi_dvb_sds_prog_ctx_nth;
	}
	return NULL;
}

psi_dvb_sds_prog_ctx_t* psi_dvb_sds_prog_ctx_allocate()
{
	psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx=
			(psi_dvb_sds_prog_ctx_t*)calloc(1, sizeof(psi_dvb_sds_prog_ctx_t));
	CHECK_DO(psi_dvb_sds_prog_ctx!= NULL, return NULL);
	return psi_dvb_sds_prog_ctx;
}

psi_dvb_sds_prog_ctx_t* psi_dvb_sds_prog_ctx_dup(
		const psi_dvb_sds_prog_ctx_t* psi_dvb_sds_prog_ctx_arg)
{
	psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx= NULL;
	int ret_code, end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(psi_dvb_sds_prog_ctx_arg!= NULL, return NULL);

	/* Allocate service description information context structure */
	psi_dvb_sds_prog_ctx= psi_dvb_sds_prog_ctx_allocate();
	CHECK_DO(psi_dvb_sds_prog_ctx!= NULL, goto end);

	/* Copy simple type data */
	psi_dvb_sds_prog_ctx->service_id= psi_dvb_sds_prog_ctx_arg->service_id;
	psi_dvb_sds_prog_ctx->EIT_schedule_flag=
			psi_dvb_sds_prog_ctx_arg->EIT_schedule_flag;
	psi_dvb_sds_prog_ctx->EIT_present_following_flag=
			psi_dvb_sds_prog_ctx_arg->EIT_present_following_flag;
	psi_dvb_sds_prog_ctx->running_status=
			psi_dvb_sds_prog_ctx_arg->running_status;
	psi_dvb_sds_prog_ctx->free_CA_mode= psi_dvb_sds_prog_ctx_arg->free_CA_mode;
	psi_dvb_sds_prog_ctx->descriptors_loop_length=
			psi_dvb_sds_prog_ctx_arg->descriptors_loop_length;

	/* Duplicate descriptors list */
	LLIST_DUPLICATE(&psi_dvb_sds_prog_ctx->psi_desc_ctx_llist,
			psi_dvb_sds_prog_ctx_arg->psi_desc_ctx_llist,
			psi_desc_ctx_dup,
			psi_desc_ctx_t, ret_code);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;

end:
	if(end_code!= STAT_SUCCESS)
		psi_dvb_sds_prog_ctx_release(&psi_dvb_sds_prog_ctx);

	return psi_dvb_sds_prog_ctx;
}

void psi_dvb_sds_prog_ctx_release(
		psi_dvb_sds_prog_ctx_t **ref_psi_dvb_sds_prog_ctx)
{
	psi_dvb_sds_prog_ctx_t *psi_dvb_sds_prog_ctx;

	if(ref_psi_dvb_sds_prog_ctx== NULL)
		return;

	if((psi_dvb_sds_prog_ctx= *ref_psi_dvb_sds_prog_ctx)!= NULL) {
		/* Release descriptors list. */
	    while(psi_dvb_sds_prog_ctx->psi_desc_ctx_llist!= NULL) {
	    	psi_desc_ctx_t *psi_desc_ctx=
	    			llist_pop(&psi_dvb_sds_prog_ctx->psi_desc_ctx_llist);
	    	psi_desc_ctx_release(&psi_desc_ctx);
	    }
		free(psi_dvb_sds_prog_ctx);
		*ref_psi_dvb_sds_prog_ctx= NULL;
	}
}
