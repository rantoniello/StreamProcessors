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
 * @file psi_desc_enc.h
 * @brief Program Specific Information (PSI) descriptors encoding module.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_SRC_PSI_DESC_ENC_H_
#define SPMPEG2TS_SRC_PSI_DESC_ENC_H_

/* **** Definitions **** */

typedef struct log_ctx_s log_ctx_t;
typedef struct psi_desc_ctx_s psi_desc_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
int psi_desc_enc(log_ctx_t *log_ctx, const psi_desc_ctx_t *psi_desc_ctx,
		void *buf);

#endif /* SPMPEG2TS_SRC_PSI_DESC_ENC_H_ */
