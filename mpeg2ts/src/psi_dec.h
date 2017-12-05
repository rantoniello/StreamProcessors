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
 * @file psi_dec.h
 * @brief Program Specific Information (PSI) decoding module.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_SRC_PSI_DEC_H_
#define SPMPEG2TS_SRC_PSI_DEC_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

typedef struct fifo_ctx_s fifo_ctx_t;
typedef struct log_ctx_s log_ctx_t;
typedef struct psi_section_ctx_s psi_section_ctx_t;

/* **** Prototypes **** */

/**
 * Get next PSI section.
 * //TODO
 */
int psi_dec_get_next_section(fifo_ctx_t* ififo_ctx, log_ctx_t *log_ctx,
		uint8_t *ref_tscc, uint16_t pid,
		psi_section_ctx_t **ref_psi_section_ctx);

/**
 * //TODO
 */
int psi_dec_section(uint8_t *buf, size_t buf_size, uint16_t pid,
		log_ctx_t *log_ctx, psi_section_ctx_t **ref_psi_section_ctx);

/**
 * //TODO
 */
int psi_dec_read_next_section(fifo_ctx_t* ififo_ctx, log_ctx_t *log_ctx,
		uint8_t *ref_tscc, void **buf, size_t *count);

#endif /* SPMPEG2TS_SRC_PSI_DEC_H_ */
