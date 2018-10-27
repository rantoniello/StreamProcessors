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
 * @file dbdriver.h
 * @brief
 * @author Rafael Antoniello
 */

#ifndef STREAMPROCESSORS_DBDRIVER_SRC_DBDRIVER_H_
#define STREAMPROCESSORS_DBDRIVER_SRC_DBDRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* **** Definitions **** */

/* Forward definitions */
typedef struct mongod_wrapper_ctx_s mongod_wrapper_ctx_t;
typedef struct log_ctx_s log_ctx_t;

/* **** prototypes **** */

/**
 * Mongo-db service wrapper. Launches Mongo-db service.
 * This function performs:
 * - Forks the calling process and "exec" the children; the children process
 * will be in charge of executing 'mongod' service in background.
 * - Internally performs 'mongoc_init()'.
 * - Waits until 'mongod' service is up and available: this is achieved by
 * performing a 'PING' and waiting until mongod responds (note that it can take
 * 5 to 10 seconds to respond).
 * @param argv Arguments to be executed by 'execve' for launching 'mongod'.
 * It is optional; if left NULL default arguments will be used.
 * @param envp Environment variables to be used by 'execve' for launching
 * 'mongod'. It is optional; if left NULL default environments will be used.
 * @param mongod_uri The URI 'mongod' uses to listen to. It is mandatory.
 * @param log_ctx LOG module context structure. It is optional.
 * @return Module's instance context structure (handler).
 */
mongod_wrapper_ctx_t* mongod_wrapper_open(char *const argv[],
		char *const envp[], const char *mongod_uri, log_ctx_t *log_ctx);

/**
 * Terminates Mongo-db service and release resources allocated in a previous
 * call to 'mongod_wrapper_open()'.
 * @param Reference to the pointer to the module's context structure allocated
 * by a previous call to 'mongod_wrapper_open()'. Pointer is set to NULL on
 * return.
 * @param log_ctx LOG module context structure. It is optional.
 */
void mongod_wrapper_close(mongod_wrapper_ctx_t **ref_mongod_wrapper_ctx,
		log_ctx_t *log_ctx);

#ifdef __cplusplus
} //extern "C"
#endif

#endif /* STREAMPROCESSORS_DBDRIVER_SRC_DBDRIVER_H_ */
