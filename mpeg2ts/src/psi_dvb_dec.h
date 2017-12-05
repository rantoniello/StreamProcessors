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
 * @file psi_dvb_dec.h
 * @brief Decoding of Program Specific Information (PSI) in DVB (Digital Video
 * Broadcasting) systems.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_SRC_PSI_DVB_DEC_H_
#define SPMPEG2TS_SRC_PSI_DVB_DEC_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

/* Forward declarations */
typedef struct psi_dvb_sds_ctx_s psi_dvb_sds_ctx_t;
typedef struct log_ctx_s log_ctx_t;
typedef struct bitparser_ctx_s bitparser_ctx_t;

/* **** Prototypes **** */

/**
 * Decode Service Description section.
 * //TODO
 */
psi_dvb_sds_ctx_t* psi_dvb_dec_sds(log_ctx_t *log_ctx,
		bitparser_ctx_t *bitparser_ctx, uint16_t pid, size_t size);

#endif /* SPMPEG2TS_SRC_PSI_DVB_DEC_H_ */
