/*
 * Copyright (c) 2015, 2016, 2017 Rafael Antoniello
 *
 * This file is part of StreamProcessor.
 *
 * StreamProcessor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StreamProcessor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with StreamProcessor.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file psi_enc.h
 * @brief PSI encoding module.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_SRC_PSI_ENC_H_
#define SPMPEG2TS_SRC_PSI_ENC_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

/* Forward declarations */
typedef struct psi_section_ctx_s psi_section_ctx_t;
typedef struct log_ctx_s log_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
int psi_section_ctx_enc(const psi_section_ctx_t *psi_section_ctx_arg,
		uint16_t pid, log_ctx_t *log_ctx, void **ref_buf, size_t *ref_buf_size);

#endif /* SPMPEG2TS_SRC_PSI_ENC_H_ */
