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
 * @file stream_procs_api_http.h
 * @brief Stream-processors module HTTP API adaptation layer.
 * @author Rafael Antoniello
 */

#ifndef STREAMPROCESSORS_MAIN_STREAM_PROCS_API_HTTP_H_
#define STREAMPROCESSORS_MAIN_STREAM_PROCS_API_HTTP_H_

#include <sys/types.h>

/* **** Definitions **** */

/* Forward declarations */
typedef struct procs_ctx_s procs_ctx_t;

/** URL-base for the API requests */
#define API_HTTP_BASE_URL "/api/1.0"

/**
 * HTTP-API request handler function.
 * @param procs_ctx
 * @param url
 * @param query_string
 * @param request_method
 * @param content
 * @param content_len
 * @param ref_str_response Reference to the pointer to a character string
 * returning the representational state associated with this request.
 * @return Status code (STAT_SUCCESS code in case of success, for other code
 * values please refer to .stat_codes.h).
 */
int stream_procs_api_http_req_handler(procs_ctx_t *procs_ctx, const char *url,
		const char *query_string, const char *request_method, char *content,
		size_t content_len, char **ref_str_response);

#endif /* STREAMPROCESSORS_MAIN_STREAM_PROCS_API_HTTP_H_ */
