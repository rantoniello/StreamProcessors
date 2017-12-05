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
 * @file psi_desc_dec.h
 * @brief Program Specific Information (PSI) descriptors decoding module.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_SRC_PSI_DESC_DEC_H_
#define SPMPEG2TS_SRC_PSI_DESC_DEC_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

typedef struct bitparser_ctx_s bitparser_ctx_t;
typedef struct log_ctx_s log_ctx_t;
typedef struct psi_desc_ctx_s psi_desc_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
psi_desc_ctx_t* psi_desc_dec(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid, size_t max_size);

#endif /* SPMPEG2TS_SRC_PSI_DESC_DEC_H_ */
