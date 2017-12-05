/*
 * Copyright (c) 2017, 2018 Rafael Antoniello
 *
 * This file is part of MediaProcessors.
 *
 * MediaProcessors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MediaProcessors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MediaProcessors. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file psi_table_dec.c
 * @author Rafael Antoniello
 */

#include "psi_table_dec.h"

#include <stdlib.h>
#include <string.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/llist.h>
#include <libmediaprocsutils/fifo.h>
#include "psi.h"
#include "psi_dec.h"
#include "psi_dvb.h"
#include "psi_dvb_dec.h"
#include "psi_desc.h"
#include "psi_table.h"

/* **** Definitions **** */

/* **** Prototypes **** */

/**
 * Auxiliary in-line function: check if table is complete; namely, if all
 * sections that compose the table are already parsed.
 */
static inline int is_table_complete(llist_t *psi_section_ctx_llist,
		log_ctx_t *log_ctx);

static int psi_table_dec_get_table_sections(fifo_ctx_t *ififo_ctx,
		log_ctx_t *log_ctx, uint8_t *ref_tscc, uint16_t pid,
		llist_t **ref_psi_section_ctx_llist);

/* **** Implementations **** */

int psi_table_dec_get_next_table(fifo_ctx_t *ififo_ctx, log_ctx_t *log_ctx,
		uint8_t *ref_tscc, uint16_t pid, psi_table_ctx_t **ref_psi_table_ctx)
{
	psi_table_ctx_t *psi_table_ctx= NULL;
	int ret_code, end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(ififo_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(ref_psi_table_ctx!= NULL, return STAT_ERROR);

	*ref_psi_table_ctx= NULL;

	/* Allocate generic table context structure */
	psi_table_ctx= psi_table_ctx_allocate();
	CHECK_DO(psi_table_ctx!= NULL, goto end);

	/* Get list of sections */
	ret_code= psi_table_dec_get_table_sections(ififo_ctx, log_ctx, ref_tscc,
			pid, &psi_table_ctx->psi_section_ctx_llist);
	if(ret_code!= STAT_SUCCESS) {
		if(ret_code== STAT_EOF)
			end_code= ret_code;
		goto end;
	}

	/* Trace table */
	// TODO: trace specific information (add to "pci.h" ...)

	*ref_psi_table_ctx= psi_table_ctx;
	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		psi_table_ctx_release(&psi_table_ctx);

	return end_code;
}

static int psi_table_dec_get_table_sections(fifo_ctx_t *ififo_ctx,
		log_ctx_t *log_ctx, uint8_t *ref_tscc, uint16_t pid,
		llist_t **ref_psi_section_ctx_llist)
{
	uint8_t  curr_table_id, curr_version_number;
	uint16_t curr_table_id_extension;
	int ret_code, end_code= STAT_ERROR;
	psi_section_ctx_t* psi_section_ctx_curr= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(ififo_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(ref_psi_section_ctx_llist!= NULL, return STAT_ERROR);

	/* Get next PSI section */
	while(psi_section_ctx_curr== NULL) {
		int ret_code= psi_dec_get_next_section(ififo_ctx, log_ctx,
				ref_tscc, pid, &psi_section_ctx_curr);
		if(ret_code== STAT_EOF) {
			end_code= ret_code;
			goto end; // We are requested to exit.
		}
	}

	/* Set current parameters */
	curr_table_id= psi_section_ctx_curr->table_id;
	curr_table_id_extension= psi_section_ctx_curr->table_id_extension;
	curr_version_number= psi_section_ctx_curr->version_number;

	/* We insert the new section in the list's position 'section_number'. In
	 * the case we complete the list, we will have the sections ordered by the
	 * 'section_number' field as follows:
	 * ['0', ..., 'last_section_number'].
	 * Note that function 'llist_insert_nth()' will append the new element at
	 * the end of list in case the given index exceeds current list size.
	 */
	ret_code= llist_insert_nth(ref_psi_section_ctx_llist,
			psi_section_ctx_curr->section_number, (void*)psi_section_ctx_curr);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);
	psi_section_ctx_curr= NULL; // avoid double referencing / freeing.

	/* Get rest of the sections that compose the table:
	 * - The table may be partitioned into up to 255 sections before it is
	 * mapped into TS packets. Each section carries a part of the overall table
	 * (e.g. refer to Rec. ITU-T H.222.0 (10/2014), Annex C.9.1.)
	 * - The section_number field allows the sections of a particular table to
	 * be reassembled in their original order by the decoder. There is no
	 * obligation that sections must be transmitted in numerical order
	 * (see Rec. ITU-T H.222.0 (10/2014), Annex C.3.)
	 */
	while(is_table_complete(*ref_psi_section_ctx_llist, LOG_CTX_GET())!=
			STAT_SUCCESS) {

		/* Get next PSI section */
		while(psi_section_ctx_curr== NULL) {
			int ret_code= psi_dec_get_next_section(ififo_ctx, LOG_CTX_GET(),
					ref_tscc, pid, &psi_section_ctx_curr);
			if(ret_code== STAT_EOF) {
				end_code= ret_code;
				goto end; // We are requested to exit.
			}
		}

		/* Check section belongs to the table being parsed */
		if(curr_table_id!= psi_section_ctx_curr->table_id ||
		   curr_table_id_extension!= psi_section_ctx_curr->table_id_extension ||
		   curr_version_number!= psi_section_ctx_curr->version_number) {
			LOGW("The table could not be completed; parsing new version\n");
			goto end;
		}

		/* Store new section in table context structure */
		ret_code= llist_insert_nth(ref_psi_section_ctx_llist,
				psi_section_ctx_curr->section_number,
				(void*)psi_section_ctx_curr);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		psi_section_ctx_curr= NULL; // avoid double referencing / freeing
	}

	end_code= STAT_SUCCESS;
end:
	/* Release temporal section context structure */
	psi_section_ctx_release(&psi_section_ctx_curr);
	return end_code;
}

static inline int is_table_complete(llist_t *psi_section_ctx_llist,
		log_ctx_t *log_ctx)
{
	psi_section_ctx_t *psi_section_ctx_0;
	LOG_CTX_INIT(log_ctx);

	if(psi_section_ctx_llist== NULL)
		return STAT_ERROR; // empty list

	/* Get the first section (at least one section should exist). */
	psi_section_ctx_0=
			(psi_section_ctx_t*)llist_get_nth(psi_section_ctx_llist, 0);
	CHECK_DO(psi_section_ctx_0!= NULL, return STAT_ERROR);

	/* Check that the sections list size is equal to 'last_section_number'
	 * field plus one (that means all sections were obtained).
	 */
	if(llist_len(psi_section_ctx_llist)!=
			(int)psi_section_ctx_0->last_section_number+ 1)
		return STAT_ERROR;

	return STAT_SUCCESS;
}
