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
 * @file mpeg2_sp.c
 * @author Rafael Antoniello
 */

#include "mpeg2_sp.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>

#include <libconfig.h>
#include <libcjson/cJSON.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocsutils/fair_lock.h>
#include <libmediaprocsutils/schedule.h>
#include <libmediaprocsutils/comm.h>
#include <libmediaprocsutils/comm_udp.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocs/procs.h>
#include <libmediaprocs/proc.h>
#include "ts.h"

/* **** Definitions **** */

/*
 * Profile core of function 'distr_thr()'.
 */
#define PROFILE_DISTR_THR // comment-me: debugging purposes

/** Installation directory complete path */
#ifndef _INSTALL_DIR //HACK: "fake" path for IDE
#define _INSTALL_DIR "./"
#endif
/** Application name */
#ifndef _PROCNAME //HACK
#define _PROCNAME ""
#endif
/** Configuration file complete path */
#define PATH_CONFIG_FILE _INSTALL_DIR"/etc/"_PROCNAME"/"_PROCNAME".conf"
/** Recovery tokens directory complete path */
#define PATH_TOKENS_PATH _INSTALL_DIR"/etc/tokens"
/** Recovery tokens file complete path */
#define PATH_RECOVER_FILE 	_INSTALL_DIR"/etc/tokens/"_PROCNAME"_recover.json"

#define NUM_DEMUXERS_MAX_POW2 16
#define NUM_DEMUXERS_MAX (1<< NUM_DEMUXERS_MAX_POW2)

/**
 * Type for processors registering and mapping.
 */
typedef struct proc_reg_s {
	fair_lock_t *lock;
	int proc_id;
} proc_reg_t;

typedef struct mpeg2_sp_settings_ctx_s {
	/**
	 * Input URL.
	 * Set to NULL to request input stream to be closed.
	 */
	char *input_url;
	/**
	 * MPEG2 stream processor tag (user customizable textual identifier).
	 */
	char *tag;
	/**
	 * Set to '1' to request to clear logs register (processor will
	 * automatically reset this flag to zero when the operation is performed).
	 */
	int flag_clear_logs;
	/**
	 * Set to '1' to request to purge all the disassociated
	 * program-processors (MPEG2 stream processor will automatically reset
	 * this flag to zero when the operation is performed).
	 */
	int flag_purge_disassociated_processors;
} mpeg2_sp_settings_ctx_t;

/**
 * MPEG2 stream processor module instance context structure.
 */
typedef struct mpeg2_sp_ctx_s {
	/**
	 * Generic processor context structure.
	 * *MUST* be the first field in order to be able to cast to proc_ctx_t.
	 */
	struct proc_ctx_s proc_ctx;
	/**
	 * MPEG2 stream processor settings.
	 */
	volatile struct mpeg2_sp_settings_ctx_s mpeg2_sp_settings_ctx;
	/**
	 * MPEG2 stream processor identifier (unambiguous number to identify this
	 * stream processor module instance).
	 */
	uint64_t id;

	/* **** ----------------------- Processors ------------------------ **** */
	/**
	 * Array to register the references to the opened processors and to map
	 * these to each input Packet-IDentifier (MPEG2-TS PID).
	 */
	proc_reg_t proc_reg_array[TS_MAX_PID_VAL+ 1];
	/**
	 * Processors module context structure.
	 * All used processors will be managed through this module instance.
	 */
	procs_ctx_t *procs_ctx;

	/* **** --------------------- Input interface --------------------- **** */
	/**
	 * Input COMM module instance context structure.
	 */
	comm_ctx_t *comm_ctx_input;
	/**
	 * Input critical section MUTEX.
	 */
	pthread_mutex_t comm_ctx_input_mutex;
	/**
	 * Packet distribution thread exit indicator.
	 * Set to non-zero to indicate distribution thread to abort immediately.
	 */
	volatile int distr_flag_exit;
	/**
	 * Distribution thread.
	 * Get new chunks of data from input interface and distribute (send) these
	 * to the corresponding processors.
	 */
	pthread_t distr_thread;

} mpeg2_sp_ctx_t;

/* **** Prototypes **** */

static proc_ctx_t* mpeg2_sp_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg);
static void mpeg2_sp_close(proc_ctx_t **ref_proc_ctx);

static int mpeg2_sp_settings_ctx_init(
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx);
static void mpeg2_sp_settings_ctx_deinit(
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx);

static void* distr_thr(void *t);

/* **** Implementations **** */

const proc_if_t proc_if_mpeg2_sp=
{
	"mpeg2_sp", "stream_processor", "n/a",
	(uint64_t)0,
	mpeg2_sp_open,
	mpeg2_sp_close,
	NULL, // 'send_frame()'
	NULL, // send-no-dup // 'send_frame_nodup()'
	NULL, // 'recv_frame()'
	NULL, // no specific unblock function extension
	mpeg2_sp_rest_put,
	mpeg2_sp_rest_get,
	NULL, // No predefined 'process_frame()' function
	NULL, // no extra options
	NULL, // 'iput_fifo_elem_opaque_dup()'
	NULL, // 'iput_fifo_elem_opaque_release()'
	NULL, // 'oput_fifo_elem_opaque_dup()'
};

/**
 * Implements the proc_if_s::open callback.
 * See .proc_if.h for further details.
 */
static proc_ctx_t* mpeg2_sp_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg)
{
	config_t cfg;
	const char *host_ipv4_addr, *p; // Do not release
	int i, ret_code, end_code= STAT_ERROR, proc_instance_index= -1;
	uint64_t id= 0;
	mpeg2_sp_ctx_t *mpeg2_sp_ctx= NULL;
	volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx=
			NULL; // Do not release
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(proc_if!= NULL, return NULL);
	CHECK_DO(settings_str!= NULL, return NULL);
	// In this processor, 'log_ctx' *MUST* be NULL as it will be defined
	// internally.
	CHECK_DO(log_ctx== NULL, return NULL); // *MUST* be NULL for this processor

	/* Parse variable arguments */
	proc_instance_index= va_arg(arg, int);
	CHECK_DO(proc_instance_index>= 0, return NULL);

	/* Allocate context structure */
	mpeg2_sp_ctx= (mpeg2_sp_ctx_t*)calloc(1, sizeof(mpeg2_sp_ctx_t));
	CHECK_DO(mpeg2_sp_ctx!= NULL, goto end);

	/* **** Special case: ****
	 * First of all we compose the stream processor ID and instantiate the
	 * processor LOG module associating this unambiguous ID.
	 */

	/* MPEG2 stream processor unique ID:
	 * - Initialize configuration file management module;
	 * - Read configuration file;
	 * - Get necessary configuration information (NOTE: All pointers to
	 * configuration resources are static and should not be freed;
	 * configuration file management context should be freed at the end).
	 * Finally we compose the ID as follows:
	 * - The 16 less-significant bits are the 'proc_instance_index';
	 * - the 64-16 most-significant bits are the host IPv4 address.
	 */
	config_init(&cfg);
	if(!config_read_file(&cfg, PATH_CONFIG_FILE))
	{
		LOGE("%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg),
				config_error_text(&cfg));
		goto end;
	}
	if(!config_lookup_string(&cfg, "http_server.local_address",
			&host_ipv4_addr)) {
		LOGE("Local address *MUST* be specified in configuration file\n");
		goto end;
	}
	for(id= 0, i= 0, p= host_ipv4_addr;
			i< 4;
			i++, p= strchr(p, '.')+ 1) {
		if(p== NULL || strlen(p)== 0) {
			LOGE("Review 'local address' specified in configuration file\n");
			goto end;
		}
		id|= ((uint32_t)atoi(p))<< ((3-i)<< 3);
	}
	id= (id<< NUM_DEMUXERS_MAX_POW2)| proc_instance_index;
	mpeg2_sp_ctx->id= id;

	/* Instantiate the processor's LOG module */
	((proc_ctx_t*)mpeg2_sp_ctx)->log_ctx= log_open(id);
	CHECK_DO(((proc_ctx_t*)mpeg2_sp_ctx)->log_ctx!= NULL, goto end);
	LOG_CTX_SET(((proc_ctx_t*)mpeg2_sp_ctx)->log_ctx);

	/* Get settings structure */
	mpeg2_sp_settings_ctx= &mpeg2_sp_ctx->mpeg2_sp_settings_ctx;

	/* Initialize settings to defaults */
	ret_code= mpeg2_sp_settings_ctx_init(mpeg2_sp_settings_ctx, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Parse and put given settings */
	ret_code= mpeg2_sp_rest_put((proc_ctx_t*)mpeg2_sp_ctx, settings_str);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* **** Initialize rest of context structure **** */

	/* Processors module context structure */
	mpeg2_sp_ctx->procs_ctx= procs_open(LOG_CTX_GET());
	CHECK_DO(mpeg2_sp_ctx->procs_ctx!= NULL, goto end);

	/* Processor units register/mapping array */
	for(i= 0; i< (TS_MAX_PID_VAL+ 1); i++) {
		mpeg2_sp_ctx->proc_reg_array[i].lock= fair_lock_open();
		CHECK_DO(mpeg2_sp_ctx->proc_reg_array[i].lock!= NULL, goto end);
		mpeg2_sp_ctx->proc_reg_array[i].proc_id=
				-1; // '-1' means not instantiated'
	}

	/* Input COMM module instance context structure */
	mpeg2_sp_ctx->comm_ctx_input= NULL; // Set by 'demuxer_opt()'

	/* Input critical section MUTEX */
	ret_code= pthread_mutex_init(&mpeg2_sp_ctx->comm_ctx_input_mutex, NULL);
	CHECK_DO(ret_code== 0, goto end);

	/* Packet distribution thread exit indicator */
	mpeg2_sp_ctx->distr_flag_exit= 0;

	/* Distribution thread */
	//pthread_t distr_thread; //launch threads at the end of this function

	// Reserved for future use: add new fields initializations here...

	/* **** Finally, launch threads **** */

	/* Launch distribution thread */
	ret_code= pthread_create(&mpeg2_sp_ctx->distr_thread, NULL, distr_thr,
			(void*)mpeg2_sp_ctx);
	CHECK_DO(ret_code== 0, goto end);

    end_code= STAT_SUCCESS;
 end:
    if(end_code!= STAT_SUCCESS)
    	mpeg2_sp_close((proc_ctx_t**)&mpeg2_sp_ctx);
	return (proc_ctx_t*)mpeg2_sp_ctx;
}

/**
 * Implements the proc_if_s::close callback.
 * See .proc_if.h for further details.
 */
static void mpeg2_sp_close(proc_ctx_t **ref_proc_ctx)
{
	int i;
	mpeg2_sp_ctx_t *mpeg2_sp_ctx;
	void *thread_end_code= NULL;
	LOG_CTX_INIT(NULL);

	if(ref_proc_ctx== NULL ||
			(mpeg2_sp_ctx= (mpeg2_sp_ctx_t*)*ref_proc_ctx)== NULL)
		return;

	LOG_CTX_SET(((proc_ctx_t*)mpeg2_sp_ctx)->log_ctx);

	/* **** Firstly join all threads **** */

	/* Join packet distribution thread:
	 * - Set flag to exit;
	 * - Unblock input COMM;
	 * - Unblock (delete/close) all registered/mapped processors;
	 */
	mpeg2_sp_ctx->distr_flag_exit= 1;

	ASSERT(comm_unblock(mpeg2_sp_ctx->comm_ctx_input)== STAT_SUCCESS);

	for(i= 0; i< (TS_MAX_PID_VAL+ 1); i++) {
		int proc_id;
		proc_reg_t *p_proc_reg= &mpeg2_sp_ctx->proc_reg_array[i];
		//fair_lock(p_proc_reg->lock); // NEVER! deadlock 'procs_send_frame()'
		proc_id= p_proc_reg->proc_id;
		if(proc_id>= 0) {
			ASSERT(procs_opt(mpeg2_sp_ctx->procs_ctx, "PROCS_ID_DELETE",
					proc_id)== STAT_SUCCESS);
		}
		//fair_unlock(p_proc_reg->lock);
	}

	LOGV("Waiting for distribution thread to join... "); //comment-me
	pthread_join(mpeg2_sp_ctx->distr_thread, &thread_end_code);
	if(thread_end_code!= NULL) {
		ASSERT(*((int*)thread_end_code)== STAT_SUCCESS);
		free(thread_end_code);
		thread_end_code= NULL;
	}
	LOGV("thread joined O.K.\n"); //comment-me

	/* **** Once joined all threads, release context structure fields **** */

	/* Release settings */
	mpeg2_sp_settings_ctx_deinit(&mpeg2_sp_ctx->mpeg2_sp_settings_ctx,
			LOG_CTX_GET());

	/* Release processor units register/mapping array */
	for(i= 0; i< (TS_MAX_PID_VAL+ 1); i++) {
		proc_reg_t *p_proc_reg= &mpeg2_sp_ctx->proc_reg_array[i];
		fair_lock_close(&p_proc_reg->lock);
		ASSERT(p_proc_reg->lock== NULL);
	}

	/* Release processors module context structure */
	procs_close(&mpeg2_sp_ctx->procs_ctx);

	/* Release input COMM module instance context structure */
	comm_close(&mpeg2_sp_ctx->comm_ctx_input);

	/* Release input critical section MUTEX */
	ASSERT(pthread_mutex_destroy(&mpeg2_sp_ctx->comm_ctx_input_mutex)== 0);

	// Reserved for future use: release other new variables here...

	/* Release context structure */
	free(mpeg2_sp_ctx);
	*ref_proc_ctx= NULL;
}

/**
 * Initialize specific MPEG2 stream processor settings to defaults.
 * @param mpeg2_sp_settings_ctx
 * @param log_ctx
 * @return Status code (STAT_SUCCESS code in case of success, for other code
 * values please refer to .stat_codes.h).
 */
static int mpeg2_sp_settings_ctx_init(
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx)
{
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(mpeg2_sp_settings_ctx!= NULL, return STAT_ERROR);

	/* Input URL */
	mpeg2_sp_settings_ctx->input_url= NULL; // Set by 'demuxer_opt()'

	/* MPEG2 stream processor tag (user customizable textual identifier) */
	mpeg2_sp_settings_ctx->tag= NULL; // Set by 'demuxer_opt()'

	/* Flag to clear logs register */
	mpeg2_sp_settings_ctx->flag_clear_logs= 0;

	/* Flag to purge disassociated processors */
	mpeg2_sp_settings_ctx->flag_purge_disassociated_processors= 0;

	// Reserved for future use

	return STAT_SUCCESS;
}

/**
 * Release specific MPEG2 stream processor settings (allocated in heap memory).
 * @param mpeg2_sp_settings_ctx
 * @param log_ctx
 */
static void mpeg2_sp_settings_ctx_deinit(
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx)
{
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(mpeg2_sp_settings_ctx!= NULL, return);

	/* Release input URL */
	if(mpeg2_sp_settings_ctx->input_url!= NULL) {
		free(mpeg2_sp_settings_ctx->input_url);
		mpeg2_sp_settings_ctx->input_url= NULL;
	}

	/* Release MPEG2 stream processor tag */
	if(mpeg2_sp_settings_ctx->tag!= NULL) {
		free(mpeg2_sp_settings_ctx->tag);
		mpeg2_sp_settings_ctx->tag= NULL;
	}

	// Reserved for future use
}

/**
 * Reads MPEG2-TS packets from input stream processor socket and distribute
 * to corresponding processors (or discard if no processor is assigned).
 */
static void* distr_thr(void *t)
{
#ifdef PROFILE_DISTR_THR
	struct timespec monotime;
	uint32_t average_counter= 0;
	int64_t profile_nsec, average_nsecs= 0;
#endif
	uint16_t pid= 0;
	mpeg2_sp_ctx_t *mpeg2_sp_ctx= (mpeg2_sp_ctx_t*)t; // Do not release
	proc_ctx_t *proc_ctx= NULL; // Do not release (alias)
	int *ref_end_code= NULL; // Do not release
	uint8_t *recv_buf= NULL;
	LOG_CTX_INIT(NULL);

	/* Allocate return context; initialize to a default 'STAT_ERROR' value */
	ref_end_code= (int*)malloc(sizeof(int));
	CHECK_DO(ref_end_code!= NULL, return NULL);
	*ref_end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(mpeg2_sp_ctx!= NULL, goto end);

	proc_ctx= (proc_ctx_t*)mpeg2_sp_ctx;

	LOG_CTX_SET(proc_ctx->log_ctx);

	while(mpeg2_sp_ctx->distr_flag_exit== 0) {
		int ret_code;
		size_t recv_buf_size= 0;
		uint8_t *pkt_p;

		/* Receive new packet of data from input interface */
		if(recv_buf!= NULL) {
			free(recv_buf);
			recv_buf= NULL;
			recv_buf_size= 0;
		}
		ret_code= comm_recv(mpeg2_sp_ctx->comm_ctx_input, (void**)&recv_buf,
				&recv_buf_size, NULL, NULL);
		if(ret_code!= STAT_SUCCESS) {
			if(ret_code!= STAT_EOF && ret_code!= STAT_ETIMEDOUT)
				LOGE("Input interface failed to receive new packet\n");
			schedule(); // schedule to avoid closed loops
			continue;
		}
		CHECK_DO(recv_buf!= NULL, schedule(); continue);

		for(pkt_p= recv_buf; recv_buf_size>= TS_PKT_SIZE;
				pkt_p+= TS_PKT_SIZE, recv_buf_size-= TS_PKT_SIZE) {
			register int proc_id;
			proc_reg_t *p_proc_reg; // Do not release

			/* Check exit point */
			if(mpeg2_sp_ctx->distr_flag_exit!= 0)
				goto end;

			pid= TS_BUF_GET_PID(pkt_p);

			/* Check sanity of received packet ("legacy" MPEG-2 TS packets) */
			if(pid> TS_MAX_PID_VAL) {
				LOGE("Stream processor received a corrupted TS input packet: "
						"erroneous PID value (%u).\n", pid);
				schedule();
				continue;
			}
			if(pkt_p[0]!= 0x47) {
				LOGE("Stream processor received a corrupted TS input packet: "
						"erroneous sync. byte (0x47). "
						"PID= %u (0x%0x)\n", pid, pid);
				schedule();
				continue;
			}

			/* Send new data packets to processor if assigned */
			p_proc_reg= &mpeg2_sp_ctx->proc_reg_array[pid];
			ret_code= STAT_SUCCESS;
			fair_lock(p_proc_reg->lock);
		    if((proc_id= p_proc_reg->proc_id)>= 0) {
#ifdef PROFILE_DISTR_THR
		    	clock_gettime(CLOCK_MONOTONIC, &monotime);
		    	profile_nsec= (int64_t)monotime.tv_sec*1000000000+
		    			(int64_t)monotime.tv_nsec;
#endif
		    	proc_frame_ctx_t proc_frame_ctx= {0};
		    	proc_frame_ctx.data= pkt_p;
		    	proc_frame_ctx.p_data[0]= pkt_p;
		    	proc_frame_ctx.linesize[0]= TS_PKT_SIZE;
		    	proc_frame_ctx.width[0]= TS_PKT_SIZE;
		    	proc_frame_ctx.height[0]= 1;
		    	proc_frame_ctx.proc_sample_fmt= PROC_IF_FMT_UNDEF;
		    	proc_frame_ctx.pts= -1;
		    	proc_frame_ctx.dts= -1;
		    	ret_code= procs_send_frame(mpeg2_sp_ctx->procs_ctx, proc_id,
		    			&proc_frame_ctx);
#ifdef PROFILE_DISTR_THR
		    	clock_gettime(CLOCK_MONOTONIC, &monotime);
		    	profile_nsec= (int64_t)monotime.tv_sec*1000000000+
		    			(int64_t)monotime.tv_nsec- profile_nsec;
		    	average_nsecs+= profile_nsec;
		    	if(++average_counter> 10000) {
		    		LOGV("Average time: %ld nsecs\n", average_nsecs/
		    				average_counter);
		    		average_counter= average_nsecs= 0;
		    	}
#endif
		    }
			fair_unlock(p_proc_reg->lock);
			if(ret_code!= STAT_SUCCESS) {
				schedule();
				continue;
			}
		}
		/* Check sanity of received packet size */
		if(recv_buf_size!= 0) {
			LOGE("Stream processor received a corrupted TS input packet: "
					"erroneous packet size (%d bytes). PID= %u (0x%0x)\n",
					recv_buf_size, pid, pid);
			schedule();
			continue;
		}
	} // distribution loop

	*ref_end_code= STAT_SUCCESS;
end:
	if(recv_buf!= NULL)
		free(recv_buf);
	return (void*)ref_end_code;
}
