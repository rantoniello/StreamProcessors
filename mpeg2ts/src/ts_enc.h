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
 * @file ts_enc.h
 * @brief MPEG-2 Transport Stream (TS) encoding module.
 * @author Rafael Antoniello
 */

#ifndef STREAMPROCESSORS_MPEG2TS_SRC_TS_ENC_H_
#define STREAMPROCESSORS_MPEG2TS_SRC_TS_ENC_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

/* Forward declarations */
typedef struct ts_ctx_s ts_ctx_t;
typedef struct log_ctx_s log_ctx_t;
typedef struct llist_s llist_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
int ts_enc_packet(const ts_ctx_t *ts_ctx, log_ctx_t *log_ctx, void **buf,
		size_t *size);

/**
 * //TODO
 */
int ts_enc_pcr_packet(log_ctx_t *log_ctx, uint16_t pid, uint8_t cc,
		int64_t pcr_base, int64_t pcr_ext, void **buf, size_t *size);

/**
 * //TODO
 */
int ts_enc_stuffing_packet(log_ctx_t *log_ctx, uint16_t pid, uint8_t cc,
		void **ref_buf, size_t *ref_size);

/**
 * //TODO
 */
int ts_enc_pcrrestamp(void *buf, int64_t pcr_base, int64_t pcr_ext);

/**
 * //TODO
 */
int ts_enc_add_adaptation_field_stuffing(ts_ctx_t *ts_ctx, uint8_t stuff_size,
		log_ctx_t *log_ctx);

#endif /* STREAMPROCESSORS_MPEG2TS_SRC_TS_ENC_H_ */
