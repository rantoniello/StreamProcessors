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
 * @file psi_table.h
 * @brief Program Specific Information (PSI) tables.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_SRC_PSI_TABLE_H_
#define SPMPEG2TS_SRC_PSI_TABLE_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

typedef struct psi_section_ctx_s psi_section_ctx_t;
typedef struct llist_s llist_t;
typedef struct psi_pas_prog_ctx_s psi_pas_prog_ctx_t;
typedef struct psi_dvb_sds_prog_ctx_s psi_dvb_sds_prog_ctx_t;
typedef struct psi_pms_es_ctx_s psi_pms_es_ctx_t;

/**
 * Generic Program Specific Information (PSI) table context structure.
 */
typedef struct psi_table_ctx_s {
	/**
	 * List of sections that compose the table (type 'psi_section_ctx_t').
	 * @see 'psi_dvb.h'
	 */
	llist_t *psi_section_ctx_llist;
} psi_table_ctx_t;

/**
 * Program Association Table (PAT) context structure.
 */
typedef struct psi_table_ctx_s psi_table_pat_ctx_t;

/**
 * Program Map Table (PMT) context structure.
 */
typedef struct psi_table_ctx_s psi_table_pmt_ctx_t;

/**
 * DVB (Digital Video Broadcasting) Service Description Table (PAT) context
 * structure.
 */
typedef struct psi_table_ctx_s psi_table_dvb_sdt_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
psi_table_ctx_t* psi_table_ctx_allocate();

/**
 * //TODO
 */
psi_table_ctx_t* psi_table_ctx_dup(const psi_table_ctx_t *psi_table_ctx);

/**
 * //TODO
 */
int psi_table_ctx_cmp(psi_table_ctx_t *psi_table_ctx1,
		psi_table_ctx_t *psi_table_ctx2);

/**
 * //TODO
 */
void psi_table_ctx_release(psi_table_ctx_t **ref_psi_table_ctx);

/**
 * //TODO
 */
psi_table_pat_ctx_t* psi_table_pat_ctx_allocate();

/**
 * //TODO
 */
psi_table_pat_ctx_t* psi_table_pat_ctx_dup(const psi_table_pat_ctx_t*
		psi_table_pat_ctx);

/**
 * //TODO
 */
int psi_table_pat_ctx_cmp(const psi_table_pat_ctx_t* psi_table_pat_ctx1,
		const psi_table_pat_ctx_t* psi_table_pat_ctx2);

/**
 * //TODO
 */
void psi_table_pat_ctx_release(psi_table_pat_ctx_t **ref_psi_table_pat_ctx);

/**
 * //TODO
 */
psi_pas_prog_ctx_t* psi_table_pat_ctx_filter_pid(
		const psi_table_pat_ctx_t* psi_table_pat_ctx, uint16_t reference_pid);

/**
 * //TODO
 */
psi_pas_prog_ctx_t* psi_table_pat_ctx_filter_program_num(
		const psi_table_pat_ctx_t* psi_table_pat_ctx, uint16_t program_number);

/**
 * //TODO
 */
psi_section_ctx_t* psi_table_pmt_ctx_filter_program_pid(
		const psi_table_pat_ctx_t* psi_table_pat_ctx,
		const psi_table_pmt_ctx_t* psi_table_pmt_ctx, uint16_t reference_pid);

/**
 * //TODO
 */
psi_pms_es_ctx_t* psi_table_pmt_ctx_filter_program_pid_es_pid(
		const psi_table_pat_ctx_t* psi_table_pat_ctx,
		const psi_table_pmt_ctx_t* psi_table_pmt_ctx, uint16_t prog_pid,
		uint16_t es_pid);

/**
 * //TODO
 */
psi_section_ctx_t* psi_table_pmt_ctx_filter_program_num(
		const psi_table_pmt_ctx_t* psi_table_pmt_ctx, uint16_t program_number);

/**
 * //TODO
 */
psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx_allocate();

/**
 * //TODO
 */
psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx_dup(
		const psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx);

/**
 * //TODO
 */
void psi_table_dvb_sdt_ctx_release(
		psi_table_dvb_sdt_ctx_t** ref_psi_table_dvb_sdt_ctx);

/**
 * Iterate over given SDT and get service specific data for a given program
 * number.
 * //TODO
 */
psi_dvb_sds_prog_ctx_t* psi_table_dvb_sdt_ctx_filter_program_num(
		const psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx,
		uint16_t program_number);

/**
 * //TODO
 */
const char* psi_table_dvb_sdt_ctx_filter_service_name_by_program_num(
		const psi_table_dvb_sdt_ctx_t* psi_table_dvb_sdt_ctx,
		uint16_t program_number);

#endif /* SPMPEG2TS_SRC_PSI_TABLE_H_ */
