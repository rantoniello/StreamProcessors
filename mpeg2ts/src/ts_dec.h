/*
 * Copyright (c) 2015, 2016, 2017, 2018 Rafael Antoniello
 *
 * This file is part of StreamProcessors.
 *
 * StreamProcessors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StreamProcessors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with StreamProcessors.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file ts_dec.h
 * @brief Transport stream layer decoder module.
 * @author Rafael Antoniello
 */

#ifndef STREAMPROCESSORS_MPEG2TS_SRC_TS_DEC_H_
#define STREAMPROCESSORS_MPEG2TS_SRC_TS_DEC_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

typedef struct fifo_ctx_s fifo_ctx_t;
typedef struct log_ctx_s log_ctx_t;
typedef struct ts_ctx_s ts_ctx_t;

#define TS_DEC_GET_PCR_BASE(PCR) ((((int64_t)(PCR))/300)&(int64_t)0x1FFFFFFFF)
#define TS_DEC_GET_PCR_EXT(PCR) (((int64_t)(PCR))%300)

#define TS_DEC_PARSE_PCR_BASE(BUF) ((uint64_t)\
		((((uint64_t)((uint8_t*)BUF)[6])<< 25)+\
		 (((uint32_t)((uint8_t*)BUF)[7])<< 17)+\
		 (((uint32_t)((uint8_t*)BUF)[8])<< 9)+\
		 (((uint32_t)((uint8_t*)BUF)[9])<< 1)+\
		 (((uint8_t*)BUF)[10]>> 7))\
		)
#define TS_DEC_PARSE_PCR_EXT(BUF) ((uint16_t)\
		((((uint16_t)((uint8_t*)BUF)[10]& 1)<< 8)+ ((uint8_t*)BUF)[11])\
		)
#define TS_DEC_GET_PCR(BUF) \
		!(((uint8_t*)BUF) &&\
	      (((uint8_t*)BUF)[3]&0x20) &&/* Adaptation field flag is set */\
		  (((uint8_t*)BUF)[4]!= 0) && /* Adaptation field length > 0*/\
		  (((uint8_t*)BUF)[5]& 0x10)/* PCR flag== 'true' */\
		 )? TS_TIMESTAMP_INVALID:\
		 (TS_DEC_PARSE_PCR_BASE(BUF)* 300)+ TS_DEC_PARSE_PCR_EXT(BUF);

/* **** Prototypes **** */

/**
 * Get next MPEG2-TS packet. Allocate the packet in an MPEG-2 TS context
 * structure (type 'ts_ctx_t').
 * @param ififo_ctx Input packet FIFO buffer context structure.
 * @param log_ctx LOG module context structure.
 * @param ref_ts_ctx Reference to the pointer to the MPEG-2 TS context
 * structure to be allocated and initialized.
 * @return Status code (refer to 'stat_codes_ctx_t' type).
 * @see stat_codes_ctx_t
 */
int ts_dec_get_next_packet(fifo_ctx_t *ififo_ctx, log_ctx_t *log_ctx,
		uint8_t *ref_cc, ts_ctx_t **ref_ts_ctx);

/**
 * //TODO
 */
int ts_dec_packet(uint8_t *pkt, log_ctx_t *log_ctx, ts_ctx_t **ref_ts_ctx);

#endif /* STREAMPROCESSORS_MPEG2TS_SRC_TS_DEC_H_ */
