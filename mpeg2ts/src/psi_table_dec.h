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
 * @file psi_table_dec.h
 * @brief Program Specific Information (PSI) table decoding module.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_PSI_TABLE_DEC_H_
#define SPMPEG2TS_PSI_TABLE_DEC_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

typedef struct fifo_ctx_s fifo_ctx_t;
typedef struct log_ctx_s log_ctx_t;
typedef struct psi_table_ctx_s psi_table_ctx_t;

/* **** Prototypes **** */

/**
 * Get next Program Specific Information (PSI) table.
 * //TODO
 */
int psi_table_dec_get_next_table(fifo_ctx_t *ififo_ctx, log_ctx_t *log_ctx,
		uint8_t *ref_tscc, uint16_t pid, psi_table_ctx_t **ref_psi_table_ctx);

#endif /* SPMPEG2TS_PSI_TABLE_DEC_H_ */
