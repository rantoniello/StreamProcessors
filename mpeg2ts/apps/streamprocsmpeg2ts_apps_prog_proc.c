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
 * @file streamprocsmpeg2ts_apps_prog_proc.c
 * @brief MPEG2-SPTS/MPTS program processor heavy weight processor.
 * @author Rafael Antoniello
 */

#include "../src/prog_proc_shm.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocs/proc.h>

/* **** Definitions **** */

/* Debugging purposes only */
//#define SIMULATE_FAIL_DB_UPDATE
#ifndef SIMULATE_FAIL_DB_UPDATE
	#define SIMULATE_CRASH()
#else
	#define SIMULATE_CRASH() exit(EXIT_FAILURE)
#endif

/**
 * Program processor settings context structure.
 */
typedef struct prog_proc_settings_ctx_s {
	// Reserved for future use
} prog_proc_settings_ctx_t;

/**
 * Program processor task (heavy weight processing) context structure.
 */
typedef struct prog_proc_tsk_ctx_s {
	/**
	 * Externally defined LOG module context structure instance.
	 */
	log_ctx_t *log_ctx;
	/**
	 * Program processor settings.
	 */
	volatile struct prog_proc_settings_ctx_s prog_proc_settings_ctx;

	/* **** ------------Inter-process communication (IPC) -------------- **** */
	/**
	 * Input/output FIFO buffers.
	 * These are defined in shared memory by the parent process and shared
	 * with this structure by coping the pointers. Thus, these FIFOs are
	 * created externally and should not be destroyed when releasing this
	 * structure.
	 */
	fifo_ctx_t *fifo_ctx_io_array[PROC_IO_NUM];
	/**
	 * Input/output API buffers.
	 * These are defined in shared memory by the parent process and shared
	 * with this structure by coping the pointers. Thus, these FIFOs are
	 * created externally and should not be destroyed when releasing this
	 * structure.
	 */
	fifo_ctx_t *fifo_ctx_api_array[PROC_IO_NUM];
	/**
	 * Processing thread exit indicator, defined in shared memory.
	 * Set to non-zero to signal processing task to abort immediately.
	 * This is defined in shared memory by the parent process and shared
	 * with this structure by coping the pointer. Thus, this flag is
	 * created externally and should not be freed when releasing this
	 * structure.
	 */
	volatile int *ref_flag_exit_shared;
} prog_proc_tsk_ctx_t;

/* **** Prototypes **** */

static prog_proc_tsk_ctx_t* prog_proc_tsk_open(const char *settings_str,
		const char* href_arg, log_ctx_t *log_ctx);
static void prog_proc_tsk_close(prog_proc_tsk_ctx_t **ref_prog_proc_tsk_ctx);

static int prog_proc_settings_ctx_init(
		volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx,
		log_ctx_t *log_ctx);
static void prog_proc_settings_ctx_deinit(
		volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx,
		log_ctx_t *log_ctx);

/* **** Implementations **** */

int main(int argc, char *argv[], char *envp[])
{
	int end_code= STAT_ERROR;
	const char *href_arg= (const char*)argv[1];
	const char *settings_str= (const char*)argv[2];
	size_t href_arg_len= 0;
	prog_proc_tsk_ctx_t *prog_proc_tsk_ctx= NULL;
	LOG_CTX_INIT(NULL);
	SIMULATE_CRASH();

	/* Check arguments */
	if(argc!= 3) {
		printf("Usage: %s <href string> <settings string>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	CHECK_DO(href_arg!= NULL && (href_arg_len= strlen(href_arg))> 0,
			exit(EXIT_FAILURE));
	CHECK_DO(settings_str!= NULL, exit(EXIT_FAILURE));

	/* Open LOG module */
	log_module_open();

	/* Instantiate the processor's LOG module */
	LOG_CTX_SET(log_open(0)); //TODO: use other Id.?
	CHECK_DO(LOG_CTX_GET()!= NULL, goto end);

	LOGD("argvc: %d; argv[0]: '%s'; href_arg: '%s'; settings_str: '%s'\n",
			argc, argv[0], href_arg, settings_str); //comment-me

	/* Get context structure */
	prog_proc_tsk_ctx= prog_proc_tsk_open(settings_str, href_arg,
			LOG_CTX_GET());
	CHECK_DO(prog_proc_tsk_ctx!= NULL, goto end);

	/* Program-processing function of our heavy weight task */
	while(*prog_proc_tsk_ctx->ref_flag_exit_shared== 0) {
		LOGD("testing-loop!!!\n");
		usleep(1000*1000);
	}

	end_code= STAT_SUCCESS;
end:
	/* **** Release task related resources **** */

	/* Release context structure */
	prog_proc_tsk_close(&prog_proc_tsk_ctx);

	/* Remember we opened our own LOG module instance... release it */
	log_close(&LOG_CTX_GET());

	/* Release LOG module */
	log_module_close();

	return (end_code== STAT_SUCCESS)? EXIT_SUCCESS: EXIT_FAILURE;
}

static prog_proc_tsk_ctx_t* prog_proc_tsk_open(const char *settings_str,
		const char* href_arg, log_ctx_t *log_ctx)
{
	char *p;
	int ret_code, end_code= STAT_ERROR, shm_fd= -1;
	size_t href_arg_len= 0;
	prog_proc_tsk_ctx_t *prog_proc_tsk_ctx= NULL;
	volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx=
			NULL; // Do not release
	const size_t fifo_ctx_maxsize[PROC_IO_NUM]= {PROG_PROC_SHM_FIFO_SIZE_IPUT,
			PROG_PROC_SHM_FIFO_SIZE_OPUT};
	const size_t fifo_io_chunk_size= PROG_PROC_SHM_SIZE_IO_CHUNK; // [bytes]
	const size_t fifo_api_chunk_size_put= PROG_PROC_SHM_SIZE_CHUNK_PUT;
	const size_t fifo_api_chunk_size_get= PROG_PROC_SHM_SIZE_CHUNK_GET;
	char href[PROCS_HREF_MAX_LEN+ 32]= {0};
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(settings_str!= NULL, return NULL);
	CHECK_DO(href_arg!= NULL && (href_arg_len= strlen(href_arg))> 0,
			return NULL);
	CHECK_DO(log_ctx!= NULL, return NULL);

	/* Allocate task context structure */
	prog_proc_tsk_ctx= (prog_proc_tsk_ctx_t*)calloc(1, sizeof(
			prog_proc_tsk_ctx_t));
	CHECK_DO(prog_proc_tsk_ctx!= NULL, goto end);

	/* Set LOGs context */
	prog_proc_tsk_ctx->log_ctx= log_ctx;

	/* Get settings structure */
	prog_proc_settings_ctx= &prog_proc_tsk_ctx->prog_proc_settings_ctx;

	/* Initialize settings to defaults */
	ret_code= prog_proc_settings_ctx_init(prog_proc_settings_ctx,
			LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Parse and put given settings */
	//ret_code= prog_proc_rest_put(prog_proc_tsk_ctx, settings_str); //FIXME!!: TODO!!
	//CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* **** Open shared memory pointers/references to communicate **** */

	/* Compose FIFO name prefix using 'href' */
	CHECK_DO(href_arg_len< PROCS_HREF_MAX_LEN, goto end);
	memcpy(href, href_arg, href_arg_len);
	p= href;
	while((p= strchr(p, '/'))!= NULL && (p< href+ PROCS_HREF_MAX_LEN)) {
		*p= '-';
	}
	LOGD("href_arg-modified: '%s'\n", href); //comment-me

	/* We have to re-map i/o FIFOs on shared memory */
	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_I);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_IPUT]= fifo_shm_exec_open(
			fifo_ctx_maxsize[PROC_IPUT], fifo_io_chunk_size, 0, href);
	CHECK_DO(prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_IPUT]!= NULL, goto end);

	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_O);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_OPUT]= fifo_shm_exec_open(
			fifo_ctx_maxsize[PROC_OPUT], fifo_io_chunk_size, 0, href);
	CHECK_DO(prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_OPUT]!= NULL, goto end);

	/* Re-map input/output API buffers */
	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_API_I);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_IPUT]= fifo_shm_exec_open(1,
			fifo_api_chunk_size_put, 0, href);
	CHECK_DO(prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_IPUT]!= NULL, goto end);

	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_API_O);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_OPUT]= fifo_shm_exec_open(1,
			fifo_api_chunk_size_get, 0, href);
	CHECK_DO(prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_OPUT]!= NULL, goto end);

	/* Re-map the processing thread exit indicator */
	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_FLG_EXIT);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	// Create the shared memory segment
	shm_fd= shm_open(href, O_RDWR, S_IRUSR | S_IWUSR);
	CHECK_DO(shm_fd>= 0,LOGE("errno: %d\n", errno); goto end);
	//Map the shared memory segment in the address space of the process
	prog_proc_tsk_ctx->ref_flag_exit_shared= (volatile int*)mmap(NULL,
			sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if(prog_proc_tsk_ctx->ref_flag_exit_shared== MAP_FAILED)
		prog_proc_tsk_ctx->ref_flag_exit_shared= NULL;
	CHECK_DO(prog_proc_tsk_ctx->ref_flag_exit_shared!= NULL, goto end);
	ASSERT(close(shm_fd)== 0);
	shm_fd= -1;

	end_code= STAT_SUCCESS;
end:
	/* We will not need file descriptor any more */
	if(shm_fd>= 0) {
		ASSERT(close(shm_fd)== 0);
	}
	if(end_code!= STAT_SUCCESS)
		prog_proc_tsk_close(&prog_proc_tsk_ctx);
	return prog_proc_tsk_ctx;
}

static void prog_proc_tsk_close(prog_proc_tsk_ctx_t **ref_prog_proc_tsk_ctx)
{
	prog_proc_tsk_ctx_t *prog_proc_tsk_ctx;
	LOG_CTX_INIT(NULL);

	if(ref_prog_proc_tsk_ctx== NULL ||
			(prog_proc_tsk_ctx= *ref_prog_proc_tsk_ctx)== NULL)
		return;

	LOG_CTX_SET(prog_proc_tsk_ctx->log_ctx);

	/* Release settings */
	prog_proc_settings_ctx_deinit(&prog_proc_tsk_ctx->prog_proc_settings_ctx,
			LOG_CTX_GET());

	/* Un-map input/output API buffers */
	fifo_shm_exec_close(&prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_IPUT]);
	fifo_shm_exec_close(&prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_OPUT]);
	fifo_shm_exec_close(&prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_IPUT]);
	fifo_shm_exec_close(&prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_OPUT]);

	/* Un-map (but *NOT* de-initialize) processing thread exit indicator.
	 * Note that shared elements are unmapped only, but de-initialized in the
	 * parent task (not in this child).
	 */
	if(prog_proc_tsk_ctx->ref_flag_exit_shared!= NULL) {
		ASSERT(munmap((void*)prog_proc_tsk_ctx->ref_flag_exit_shared,
				sizeof(int))== 0);
	}

	free(prog_proc_tsk_ctx);
	*ref_prog_proc_tsk_ctx= NULL;
}

/**
 * Initialize specific program processor settings to defaults.
 * @param prog_proc_settings_ctx
 * @param log_ctx
 * @return Status code (STAT_SUCCESS code in case of success, for other code
 * values please refer to .stat_codes.h).
 */
static int prog_proc_settings_ctx_init(
		volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx,
		log_ctx_t *log_ctx)
{
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(prog_proc_settings_ctx!= NULL, return STAT_ERROR);

	/* Initialize specific processor settings */
	// Reserved for future use

	return STAT_SUCCESS;
}

/**
 * Release specific program processor settings (allocated in heap memory).
 * @param prog_proc_settings_ctx
 * @param log_ctx
 */
static void prog_proc_settings_ctx_deinit(
		volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx,
		log_ctx_t *log_ctx)
{
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(prog_proc_settings_ctx!= NULL, return);

	/* Release specific processor settings */
	// Reserved for future use
}
