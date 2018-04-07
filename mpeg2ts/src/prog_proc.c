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
#include "prog_proc_shm.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <errno.h>

#include <libcjson/cJSON.h>
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocs/proc.h>

/* **** Definitions **** */

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
	 * Child heavy-weight process ('task') PID.
	 */
	pid_t child_pid;

	/* **** ------------Inter-process communication (IPC) -------------- ****
	 * We will use 4 FIFO's and 1 independent flag:
	 * 1) proc_ctx_s::fifo_ctx_array[PROC_IPUT]
	 * 2) proc_ctx_s::fifo_ctx_array[PROC_OPUT]
	 * 3) prog_proc_ctx_s::fifo_ctx_api_array[PROC_IPUT]
	 * -defined immediately below-
	 * 4) prog_proc_ctx_s::fifo_ctx_api_array[PROC_OPUT]
	 * -defined immediately below-
	 * 5) prog_proc_ctx_s::ref_flag_exit_shared
	 */
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

} prog_proc_ctx_t;

/* **** Prototypes **** */

static proc_ctx_t* prog_proc_open(const proc_if_t *proc_if,
		const char *settings_str, const char* href_arg, log_ctx_t *log_ctx,
		va_list arg);
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

/* ** NOTE **
 * Functions 'prog_proc_settings_ctx_init()' and
 * 'prog_proc_settings_ctx_deinit()' are executed in the forked processor.
 * **********
 */

static int prog_proc_open_tsk(prog_proc_ctx_t *prog_proc_ctx,
		const char *settings_str, const char* href_arg);

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
	(void*(*)(const proc_frame_ctx_t*))proc_frame_ctx_dup,
	(void(*)(void**))proc_frame_ctx_release,
	(proc_frame_ctx_t*(*)(const void*))proc_frame_ctx_dup
};

/**
 * Implements the proc_if_s::open callback.
 * See .proc_if.h for further details.
 */
static proc_ctx_t* prog_proc_open(const proc_if_t *proc_if,
		const char *settings_str, const char* href_arg, log_ctx_t *log_ctx,
		va_list arg)
{
	int ret_code, end_code= STAT_ERROR, shm_fd= -1;
	prog_proc_ctx_t *prog_proc_ctx= NULL;
	proc_ctx_t *proc_ctx= NULL; // Do not release (alias)
	const size_t fifo_ctx_maxsize[PROC_IO_NUM]= {PROG_PROC_SHM_FIFO_SIZE_IPUT,
			PROG_PROC_SHM_FIFO_SIZE_OPUT};
	const size_t fifo_io_chunk_size= PROG_PROC_SHM_SIZE_IO_CHUNK; // [bytes]
	const size_t fifo_api_chunk_size_put= PROG_PROC_SHM_SIZE_CHUNK_PUT;
	const size_t fifo_api_chunk_size_get= PROG_PROC_SHM_SIZE_CHUNK_GET;
	char href[PROCS_HREF_MAX_LEN+ 32]= {0};
	size_t href_arg_len= 0;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(proc_if!= NULL, return NULL);
	CHECK_DO(settings_str!= NULL, return NULL);
	// Parameter 'href_arg' is allowed to be NULL
	// Parameter 'log_ctx' is allowed to be NULL

	/* Allocate task context structure */
	prog_proc_ctx= (prog_proc_ctx_t*)calloc(1, sizeof(prog_proc_ctx_t));
	CHECK_DO(prog_proc_ctx!= NULL, goto end);
	proc_ctx= (proc_ctx_t*)prog_proc_ctx;

	/* Compose FIFO name prefix using 'href' */
	if(href_arg!= NULL && (href_arg_len= strlen(href_arg))> 0) {
		char *p= href;
		CHECK_DO(href_arg_len< PROCS_HREF_MAX_LEN, goto end);
		memcpy(href, href_arg, href_arg_len);
		while((p= strchr(p, '/'))!= NULL && (p< href+ PROCS_HREF_MAX_LEN)) {
			*p= '-';
		}
		LOGD("href_arg-modified: '%s'\n", href); //comment-me
	}

	/* We have to reallocate i/o FIFOs on shared memory */
	fifo_close(&proc_ctx->fifo_ctx_array[PROC_IPUT]);
	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_I);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	proc_ctx->fifo_ctx_array[PROC_IPUT]= fifo_shm_open(
			fifo_ctx_maxsize[PROC_IPUT], fifo_io_chunk_size, 0, href);
	CHECK_DO(proc_ctx->fifo_ctx_array[PROC_IPUT]!= NULL, goto end);

	fifo_close(&proc_ctx->fifo_ctx_array[PROC_OPUT]);
	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_O);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	proc_ctx->fifo_ctx_array[PROC_OPUT]= fifo_shm_open(
			fifo_ctx_maxsize[PROC_OPUT], fifo_io_chunk_size, 0, href);
	CHECK_DO(proc_ctx->fifo_ctx_array[PROC_OPUT]!= NULL, goto end);

	/* Initialize input/output API buffers */
	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_API_I);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	prog_proc_ctx->fifo_ctx_api_array[PROC_IPUT]= fifo_shm_open(1,
			fifo_api_chunk_size_put, 0, href);
	CHECK_DO(prog_proc_ctx->fifo_ctx_api_array[PROC_IPUT]!= NULL, goto end);

	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_API_O);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	prog_proc_ctx->fifo_ctx_api_array[PROC_OPUT]= fifo_shm_open(1,
			fifo_api_chunk_size_get, 0, href);
	CHECK_DO(prog_proc_ctx->fifo_ctx_api_array[PROC_OPUT]!= NULL, goto end);

	/* Allocate and initialize processing thread exit indicator */
	snprintf(&href[href_arg_len], sizeof(href)- href_arg_len, SUF_SH_FLG_EXIT);
	LOGD("href_arg-modified: '%s'\n", href); //comment-me
	// Create the shared memory segment
	shm_fd= shm_open(href, O_CREAT| O_RDWR, S_IRUSR | S_IWUSR);
	CHECK_DO(shm_fd>= 0,LOGE("errno: %d\n", errno); goto end);
	// Configure size of the shared memory segment
	CHECK_DO(ftruncate(shm_fd, sizeof(int))== 0, goto end);
	//Map the shared memory segment in the address space of the process
	prog_proc_ctx->ref_flag_exit_shared= (volatile int*)mmap(NULL, sizeof(int),
			PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if(prog_proc_ctx->ref_flag_exit_shared== MAP_FAILED)
		prog_proc_ctx->ref_flag_exit_shared= NULL;
	CHECK_DO(prog_proc_ctx->ref_flag_exit_shared!= NULL, goto end);
	*prog_proc_ctx->ref_flag_exit_shared= 0;
	ASSERT(close(shm_fd)== 0);
	shm_fd= -1;

	/* Fork */
	ret_code= prog_proc_open_tsk(prog_proc_ctx, settings_str, href_arg);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
	/* We will not need file descriptor any more */
	if(shm_fd>= 0) {
		ASSERT(close(shm_fd)== 0);
	}
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
	 * - set (memory-shared)flag to notify we are exiting processing;
	 * - unlock (memory-shared) input/output frame FIFO's;
	 * - unlock (memory-shared) input/output API FIFO's;
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
    LOGD("... waited returned O.K.\n"); // comment-me

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

static int prog_proc_open_tsk(prog_proc_ctx_t *prog_proc_ctx,
		const char *settings_str, const char* href_arg)
{
	int ret_code;
	pid_t child_pid= 0; // process ID
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(prog_proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(settings_str!= NULL, return STAT_ERROR);
	CHECK_DO(href_arg!= NULL && strlen(href_arg)> 0, return STAT_ERROR);

	/* Firstly fork process */
	child_pid= fork();
	if(child_pid< 0) {
		LOGE("Could not fork program process\n");
		return STAT_ERROR;
	} else if(child_pid== 0) {
		/* **** CHILD CODE ('child_pid'== 0) **** */
		char *const args[] = {
				_INSTALL_DIR"/bin/streamprocsmpeg2ts_apps_prog_proc",
				(char *const)href_arg,
				(char *const)settings_str,
				NULL
		};
		char *const envs[] = {
				"LD_LIBRARY_PATH="_INSTALL_DIR"/lib",
				NULL
		};
		if((ret_code= execve(
				_INSTALL_DIR"/bin/streamprocsmpeg2ts_apps_prog_proc", args,
				envs))< 0) { // execve won't return if succeeded
			ASSERT(0);
			exit(ret_code);
		}
	}

	/* PARENT CODE (child_pid> 0) */
	prog_proc_ctx->child_pid= child_pid;
	return STAT_SUCCESS;
}
