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
 * @file ts.c
 * @author Rafael Antoniello
 */

#include "ts.h"

#include <stdlib.h>
#include <string.h>

#define LOG_CTX_DEFULT
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>

/* **** Definitions **** */

/* **** Implementations **** */

ts_ctx_t* ts_ctx_allocate()
{
	return (ts_ctx_t*)calloc(1, sizeof(ts_ctx_t));
}

ts_ctx_t* ts_ctx_dup(const ts_ctx_t* ts_ctx_arg)
{
	register uint16_t payload_size;
	register uint8_t *payload;
	int end_code= STAT_ERROR;
	ts_ctx_t *ts_ctx= NULL;

	/* Check arguments */
	CHECK_DO(ts_ctx_arg!= NULL, return NULL);

	/* Allocate section context structure */
	ts_ctx= ts_ctx_allocate();
	CHECK_DO(ts_ctx!= NULL, goto end);

	/* Copy all structure members at the exception of pointer values */
	memcpy(ts_ctx, ts_ctx_arg, sizeof(ts_ctx_t));
	ts_ctx->adaptation_field= NULL;
	ts_ctx->payload= NULL;

	/* Copy adaptation field (AF). */
	if(ts_ctx_arg->adaptation_field!= NULL)
		ts_ctx->adaptation_field= ts_af_ctx_dup(ts_ctx_arg->adaptation_field);

	/* Copy payload. */
	payload_size= (uint16_t)ts_ctx_arg->payload_size;
	if(ts_ctx_arg->payload_size_accum> payload_size)
		payload_size= ts_ctx_arg->payload_size_accum;
	payload= ts_ctx_arg->payload;
	if(payload_size> 0) {
		uint8_t *dst= NULL;
		CHECK_DO(payload!= NULL, goto end);
		dst= ts_ctx->payload= (uint8_t*)malloc(payload_size);
		CHECK_DO(dst!= NULL, goto end);
		memcpy(dst, payload, payload_size);
	}

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		ts_ctx_release(&ts_ctx);
	return ts_ctx;
}

void ts_ctx_trace(const ts_ctx_t *ts_ctx)
{
	CHECK_DO(ts_ctx!= NULL, return);

	LOG("> ====== TS packet Header (most significant fields) ======\n");
	LOG("> transport error indicator: %u\n",
			ts_ctx->transport_error_indicator);
	LOG("> pay-load unit start indicator: %u\n",
			ts_ctx->payload_unit_start_indicator);
	LOG("> pid: %u (0x%0x)\n", ts_ctx->pid, ts_ctx->pid);
	LOG("> scrambling control: %u\n", ts_ctx->scrambling_control);
	LOG("> continuity counter: %u\n", ts_ctx->continuity_counter);
	if(ts_ctx->adaptation_field!= NULL) {
		ts_af_ctx_t* ts_af_ctx= ts_ctx->adaptation_field;
		LOG("> Adaptation field exist (length: %u)\n",
				ts_af_ctx->adaptation_field_length+ 1);
		if(ts_af_ctx->pcr_flag) LOG("> PCR: %" PRIu64 "\n", ts_af_ctx->pcr);
		if(ts_af_ctx->opcr_flag) LOG("> OPCR: %" PRIu64 "\n", ts_af_ctx->opcr);
		if(ts_af_ctx->af_remaining && ts_af_ctx->af_remaining_size)
			LOG_TRACE_BYTE_TABLE("Rest of AF bytes", ts_af_ctx->af_remaining,
					ts_af_ctx->af_remaining_size, 16);
	} else
		LOG("> No adaptation field\n");

	if(ts_ctx->payload) {
		uint16_t payload_size= (uint16_t)ts_ctx->payload_size;
		if(ts_ctx->payload_size_accum> payload_size)
			payload_size= ts_ctx->payload_size_accum;
		LOG("> Payload exist (size= %u; acc. size: %u)\n",
				ts_ctx->payload_size, ts_ctx->payload_size_accum);
		LOG_TRACE_BYTE_TABLE("TS payload bytes", ts_ctx->payload,
				payload_size, 16);
	} else {
		LOG("> No payload\n");
	}
	LOG(">\n");
}

void ts_ctx_release(ts_ctx_t **ref_ts_ctx)
{
	ts_ctx_t *ts_ctx;

	if(ref_ts_ctx== NULL)
		return;

	if((ts_ctx= *ref_ts_ctx)!= NULL) {
		ts_af_ctx_release(&ts_ctx->adaptation_field);
		if(ts_ctx->payload!= NULL) {
			free(ts_ctx->payload);
			ts_ctx->payload= NULL;
		}
		free(ts_ctx);
		*ref_ts_ctx= NULL;
	}
}

ts_af_ctx_t* ts_af_ctx_allocate()
{
	return (ts_af_ctx_t*)calloc(1, sizeof(ts_af_ctx_t));
}

ts_af_ctx_t* ts_af_ctx_dup(const ts_af_ctx_t* ts_af_ctx_arg)
{
	register uint8_t af_remaining_size;
	register uint8_t *af_remaining;
	ts_af_ctx_t *ts_af_ctx= NULL;
	int end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(ts_af_ctx_arg!= NULL, return NULL);

	/* Allocate section context structure */
	ts_af_ctx= ts_af_ctx_allocate();
	CHECK_DO(ts_af_ctx!= NULL, goto end);

	/* Copy all structure members at the exception of pointer values */
	memcpy(ts_af_ctx, ts_af_ctx_arg, sizeof(ts_af_ctx_t));
	ts_af_ctx->af_remaining= NULL;

	/* Copy adaptation field (AF) "remaining bytes". */
	af_remaining_size= ts_af_ctx_arg->af_remaining_size;
	af_remaining= ts_af_ctx_arg->af_remaining;
	if(af_remaining_size> 0) {
		uint8_t *dst= NULL;
		CHECK_DO(af_remaining!= NULL, goto end);
		dst= ts_af_ctx->af_remaining= (uint8_t*)malloc(af_remaining_size);
		CHECK_DO(dst!= NULL, goto end);
		memcpy(dst, af_remaining, af_remaining_size);
	}

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		ts_af_ctx_release(&ts_af_ctx);
	return ts_af_ctx;
}

void ts_af_ctx_release(ts_af_ctx_t **ref_ts_af_ctx)
{
	ts_af_ctx_t *ts_af_ctx;

	if(ref_ts_af_ctx== NULL)
		return;

	if((ts_af_ctx= *ref_ts_af_ctx)!= NULL) {
		if(ts_af_ctx->af_remaining!= NULL) {
			free(ts_af_ctx->af_remaining);
			ts_af_ctx->af_remaining= NULL;
		}
		free(ts_af_ctx);
		*ref_ts_af_ctx= NULL;
	}
}

int ts_get_header_size(const ts_ctx_t* ts_ctx, uint8_t *hdr_size)
{
	register uint8_t size;

	CHECK_DO(ts_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(hdr_size!= NULL, return STAT_ERROR);

	*hdr_size= 0;

	size= TS_PKT_PREFIX_LEN;
	if(ts_ctx->adaptation_field_exist) {
		register ts_af_ctx_t *ts_af_ctx= ts_ctx->adaptation_field;
		CHECK_DO(ts_af_ctx!= NULL, return STAT_ERROR);
		size+= 1+ ts_af_ctx->adaptation_field_length;
	}
	*hdr_size= size;
	return STAT_SUCCESS;
}
