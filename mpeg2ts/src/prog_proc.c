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
 * @file prog_proc.c
 * @author Rafael Antoniello
 */

#include "prog_proc.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <libcjson/cJSON.h>
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocs/proc.h>

/* **** Definitions **** */

#define ENABLE_DEBUG_LOGS
#ifdef ENABLE_DEBUG_LOGS
	#define LOGD_CTX_INIT(CTX) LOG_CTX_INIT(CTX)
	#define LOGD(FORMAT, ...) LOGV(FORMAT, ##__VA_ARGS__)
#else
	#define LOGD_CTX_INIT(CTX)
	#define LOGD(...)
#endif

/**
 * Program processor settings context structure.
 */
typedef struct prog_proc_settings_ctx_s {
	// Reserved for future use
} prog_proc_settings_ctx_t;

/**
 * Program processor context structure.
 */
typedef struct prog_proc_ctx_s {
	/**
	 * Generic processor context structure.
	 * *MUST* be the first field in order to be able to cast to proc_ctx_t.
	 */
	struct proc_ctx_s proc_ctx;
	/**
	 * Input/output API buffers.
	 * These are defined in shared memory; the pointers will be shared by
	 * coping these to 'prog_proc_tsk_ctx_t' type.
	 */
	fifo_ctx_t *fifo_ctx_api_array[PROC_IO_NUM];
	/**
	 * Processing thread exit indicator, defined in shared memory.
	 * Set to non-zero to signal processing task to abort immediately.
	 * A pointer to this flag will be shared with the forked child task.
	 */
	volatile int *ref_flag_exit_shared;
	/**
	 * Child heavy-weight process ('task') PID.
	 */
	pid_t child_pid;
} prog_proc_ctx_t;

/**
 * Program processor task (heavy weight processing) context structure.
 */
typedef struct prog_proc_tsk_ctx_s {
	/**
	 * Program processor settings.
	 */
	volatile struct prog_proc_settings_ctx_s prog_proc_settings_ctx;
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

static proc_ctx_t* prog_proc_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg);
static void prog_proc_close(proc_ctx_t **ref_proc_ctx);
/*static int prog_proc_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t *iput_fifo_ctx, fifo_ctx_t *oput_fifo_ctx);
*/ //TODO
static int prog_proc_unblock(proc_ctx_t *proc_ctx);
/*
static int prog_proc_rest_put(proc_ctx_t *proc_ctx, const char *str);
static int prog_proc_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse);
*/ //TODO
static int prog_proc_settings_ctx_init(
		volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx,
		log_ctx_t *log_ctx);
static void prog_proc_settings_ctx_deinit(
		volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx,
		log_ctx_t *log_ctx);

static int prog_proc_open_tsk(prog_proc_ctx_t *prog_proc_ctx,
		const char *settings_str, fifo_ctx_t *fifo_ctx_io_array[PROC_IO_NUM],
		fifo_ctx_t *fifo_ctx_api_array[PROC_IO_NUM],
		volatile int *ref_flag_exit_shared);

/* **** Implementations **** */

const proc_if_t proc_if_mpeg2_prog_proc= //FIXME!!
{
	"prog_proc", "demultiplexer", "n/a",
	(uint64_t)0, //(PROC_FEATURE_BITRATE|PROC_FEATURE_REGISTER_PTS),
	prog_proc_open,
	prog_proc_close,
	NULL, //prog_proc_send // TODO // similar to 'proc_send_frame_default1' but do not use frame!! just pass the frame binary-buffer
	NULL, // send-no-dup
	NULL, //prog_proc_recv // TODO
	prog_proc_unblock,
	NULL, //prog_proc_rest_put, //TODO
	NULL, //prog_proc_rest_get, //TODO
	NULL, // no processing thread, we will use a fork()! 'prog_proc_open()'.
	NULL, // no extra options
	NULL, // use default 'proc_frame_ctx_dup'
	NULL, // use default 'proc_frame_ctx_release'
	NULL, // use default 'proc_frame_ctx_dup'
};

/**
 * Implements the proc_if_s::open callback.
 * See .proc_if.h for further details.
 */
static proc_ctx_t* prog_proc_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg)
{
	int ret_code, end_code= STAT_ERROR;
	prog_proc_ctx_t *prog_proc_ctx= NULL;
	proc_ctx_t *proc_ctx= NULL; // Do not release (alias)
	const size_t fifo_ctx_maxsize[PROC_IO_NUM]= {256, 256};
	const size_t fifo_io_chunk_size= 256; // [bytes]
	const size_t fifo_api_chunk_size_put= 8192;
	const size_t fifo_api_chunk_size_get= 1024* 512;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(proc_if!= NULL, return NULL);
	CHECK_DO(settings_str!= NULL, return NULL);
	// Note: 'log_ctx' is allowed to be NULL

	/* Allocate task context structure */
	prog_proc_ctx= (prog_proc_ctx_t*)calloc(1, sizeof(prog_proc_ctx_t));
	CHECK_DO(prog_proc_ctx!= NULL, goto end);
	proc_ctx= (proc_ctx_t*)prog_proc_ctx;

	/* We have to reallocate i/o FIFOs on shared memory */
	fifo_close(&proc_ctx->fifo_ctx_array[PROC_IPUT]);
	proc_ctx->fifo_ctx_array[PROC_IPUT]= fifo_open(fifo_ctx_maxsize[PROC_IPUT],
			fifo_io_chunk_size, FIFO_PROCESS_SHARED, NULL);
	CHECK_DO(proc_ctx->fifo_ctx_array[PROC_IPUT]!= NULL, goto end);

	fifo_close(&proc_ctx->fifo_ctx_array[PROC_OPUT]);
	proc_ctx->fifo_ctx_array[PROC_OPUT]= fifo_open(fifo_ctx_maxsize[PROC_OPUT],
			fifo_io_chunk_size, FIFO_PROCESS_SHARED, NULL);
	CHECK_DO(proc_ctx->fifo_ctx_array[PROC_OPUT]!= NULL, goto end);

	/* Initialize input/output API buffers */
	prog_proc_ctx->fifo_ctx_api_array[PROC_IPUT]= fifo_open(1,
			fifo_api_chunk_size_put, FIFO_PROCESS_SHARED, NULL);
	CHECK_DO(prog_proc_ctx->fifo_ctx_api_array[PROC_IPUT]!= NULL, goto end);

	prog_proc_ctx->fifo_ctx_api_array[PROC_OPUT]= fifo_open(1,
			fifo_api_chunk_size_get, FIFO_PROCESS_SHARED, NULL);
	CHECK_DO(prog_proc_ctx->fifo_ctx_api_array[PROC_OPUT]!= NULL, goto end);

	/* Allocate and initialize processing thread exit indicator */
	prog_proc_ctx->ref_flag_exit_shared= (volatile int*)mmap(NULL, sizeof(int),
			PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if(prog_proc_ctx->ref_flag_exit_shared== MAP_FAILED)
		prog_proc_ctx->ref_flag_exit_shared= NULL;
	CHECK_DO(prog_proc_ctx->ref_flag_exit_shared!= NULL, goto end);

	/* Fork */
	ret_code= prog_proc_open_tsk(prog_proc_ctx, settings_str,
			proc_ctx->fifo_ctx_array, prog_proc_ctx->fifo_ctx_api_array,
			prog_proc_ctx->ref_flag_exit_shared);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

    end_code= STAT_SUCCESS;
 end:
    if(end_code!= STAT_SUCCESS)
    	prog_proc_close((proc_ctx_t**)&prog_proc_ctx);
	return (proc_ctx_t*)prog_proc_ctx;
}

/**
 * Implements the proc_if_s::close callback.
 * See .proc_if.h for further details.
 */
static void prog_proc_close(proc_ctx_t **ref_proc_ctx)
{
	int status;
	pid_t w;
	prog_proc_ctx_t *prog_proc_ctx= NULL;
	proc_ctx_t *proc_ctx= NULL; // Do not release (alias)
	LOG_CTX_INIT(NULL);

	if(ref_proc_ctx== NULL ||
			(prog_proc_ctx= (prog_proc_ctx_t*)*ref_proc_ctx)== NULL)
		return;

	proc_ctx= (proc_ctx_t*)prog_proc_ctx;
	LOG_CTX_SET(proc_ctx->log_ctx);

	/* Join processing task first:
	 * - set flag to notify we are exiting processing;
	 * - unlock input/output frame FIFO's;
	 * - unlock input/output API FIFO's;
	 * - wait for task to terminate.
	 */
	*prog_proc_ctx->ref_flag_exit_shared= 1;
	// NOTE that proc_ctx->fifo_ctx_array[i/o] were already unlocked and
	// closed by PROC module (refer to the calling function 'proc_close()').
	//fifo_set_blocking_mode(proc_ctx->fifo_ctx_array[PROC_IPUT], 0);
	//fifo_set_blocking_mode(proc_ctx->fifo_ctx_array[PROC_OPUT], 0);
	fifo_set_blocking_mode(prog_proc_ctx->fifo_ctx_api_array[PROC_IPUT], 0);
	fifo_set_blocking_mode(prog_proc_ctx->fifo_ctx_api_array[PROC_OPUT], 0);
	LOGD("Waiting processor task to terminate... "); // comment-me
    w= waitpid(prog_proc_ctx->child_pid, &status, WUNTRACED);
    ASSERT(w>= 0);
    if(w>= 0) {
        if(WIFEXITED(status)) {
        	LOGD("task exited, status=%d\n", WEXITSTATUS(status));
        } else if(WIFSIGNALED(status)) {
        	LOGD("task killed by signal %d\n", WTERMSIG(status));
        } else if(WIFSTOPPED(status)) {
        	LOGD("task stopped by signal %d\n", WSTOPSIG(status));
        } else if(WIFCONTINUED(status)) {
        	LOGD("task continued\n");
        }
    }

	/* Release input/output API buffers */
	fifo_close(&prog_proc_ctx->fifo_ctx_api_array[PROC_IPUT]);
	fifo_close(&prog_proc_ctx->fifo_ctx_api_array[PROC_OPUT]);

	/* Release processing thread exit indicator */
	if(prog_proc_ctx->ref_flag_exit_shared!= NULL) {
		ASSERT(munmap((void*)prog_proc_ctx->ref_flag_exit_shared, sizeof(int))
				== 0);
		prog_proc_ctx->ref_flag_exit_shared= NULL;
	}

	// Reserved for future use: release other new variables here...

	/* Release context structure */
	free(prog_proc_ctx);
	*ref_proc_ctx= NULL;
}

/**
 * Implements the proc_if_s::unblock callback.
 * See .proc_if.h for further details.
 */
static int prog_proc_unblock(proc_ctx_t *proc_ctx)
{
	prog_proc_ctx_t *prog_proc_ctx= (prog_proc_ctx_t*)proc_ctx;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	/* Unlock shared input/output FIFO's */
	fifo_set_blocking_mode(proc_ctx->fifo_ctx_array[PROC_IPUT], 0);
	fifo_set_blocking_mode(proc_ctx->fifo_ctx_array[PROC_OPUT], 0);

	/* Unlock shared input/output API FIFO's */
	fifo_set_blocking_mode(prog_proc_ctx->fifo_ctx_api_array[PROC_IPUT], 0);
	fifo_set_blocking_mode(prog_proc_ctx->fifo_ctx_api_array[PROC_OPUT], 0);

	return STAT_SUCCESS;
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

static int prog_proc_open_tsk(prog_proc_ctx_t *prog_proc_ctx,
		const char *settings_str, fifo_ctx_t *fifo_ctx_io_array[PROC_IO_NUM],
		fifo_ctx_t *fifo_ctx_api_array[PROC_IO_NUM],
		volatile int *ref_flag_exit_shared)
{
	int ret_code, exit_code= EXIT_FAILURE;
	pid_t child_pid= 0; // process ID
	prog_proc_tsk_ctx_t *prog_proc_tsk_ctx= NULL;
	volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx=
			NULL; // Do not release
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(prog_proc_ctx!= NULL, goto end);
	CHECK_DO(settings_str!= NULL, goto end);
	CHECK_DO(fifo_ctx_io_array!= NULL, goto end);
	CHECK_DO(fifo_ctx_api_array!= NULL, goto end);
	CHECK_DO(ref_flag_exit_shared!= NULL, goto end);

	/* Firstly fork process */
	child_pid= fork();
	if(child_pid< 0) {
		LOGE("Could not fork program process\n");
		return STAT_ERROR;
	} else if(child_pid> 0) {
		/* PARENT CODE */
		prog_proc_ctx->child_pid= child_pid;
		return STAT_SUCCESS;
	}

	/* **** CHILD CODE ('child_pid'== 0) **** */

	/* Allocate task context structure */
	prog_proc_tsk_ctx= (prog_proc_tsk_ctx_t*)calloc(1, sizeof(
			prog_proc_tsk_ctx_t));
	CHECK_DO(prog_proc_tsk_ctx!= NULL, goto end);

	/* Get settings structure */
	prog_proc_settings_ctx= &prog_proc_tsk_ctx->prog_proc_settings_ctx;

	/* Initialize settings to defaults */
	ret_code= prog_proc_settings_ctx_init(prog_proc_settings_ctx,
			LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Parse and put given settings */
	//ret_code= prog_proc_rest_put(prog_proc_tsk_ctx, settings_str); //FIXME!!: TODO!!
	//CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Copy shared memory pointers/references to communicate */
	prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_IPUT]=
			fifo_ctx_io_array[PROC_IPUT];
	prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_OPUT]=
			fifo_ctx_io_array[PROC_OPUT];

	prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_IPUT]=
			fifo_ctx_api_array[PROC_IPUT];
	prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_OPUT]=
			fifo_ctx_api_array[PROC_OPUT];

	prog_proc_tsk_ctx->ref_flag_exit_shared= ref_flag_exit_shared;

	/* Program-processing function of out heavy weight task */
	while(*ref_flag_exit_shared== 0) {
		LOGD("testing-loop!!!\n");
		usleep(1000*1000);
	}

	exit_code= EXIT_SUCCESS;
end:
	/* **** Release task related resources **** */

	/* Release settings */
	prog_proc_settings_ctx_deinit(&prog_proc_tsk_ctx->prog_proc_settings_ctx,
			LOG_CTX_GET());
	exit(exit_code);
}
