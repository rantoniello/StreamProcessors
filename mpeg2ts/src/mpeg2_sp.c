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
#include <sys/wait.h>

#include <libconfig.h>
#include <libcjson/cJSON.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocsutils/llist.h>
#include <libmediaprocsutils/fair_lock.h>
#include <libmediaprocsutils/schedule.h>
#include <libmediaprocsutils/interr_usleep.h>
#include <libmediaprocsutils/comm.h>
#include <libmediaprocsutils/comm_udp.h>
#include <libmediaprocsutils/uri_parser.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocs/procs.h>
#include <libmediaprocs/proc.h>
#include "ts.h"
#include "psi.h"
#include "psi_table.h"
#include "psi_dvb.h"
#include "psi_proc.h"

/* **** Definitions **** */

/*
 * Profile core of function 'distr_thr()'.
 */
#define PROFILE_DISTR_THR // comment-me: debugging purposes

/** Installation directory complete path */
#ifndef _INSTALL_DIR //HACK: "fake" path for IDE
#define _INSTALL_DIR "./"
#endif

/** Stream Processors library configuration file directory complete path */
#ifndef _CONFIG_FILE_DIR //HACK: "fake" path for IDE
#define _CONFIG_FILE_DIR "./"
#endif

#define NUM_DEMUXERS_MAX_POW2 16
#define NUM_DEMUXERS_MAX (1<< NUM_DEMUXERS_MAX_POW2)

/**
 * PSI tracking and statistics thread period [usecs]
 */
#define PSI_THREAD_PERIOD_USECS (1000* 1000)

/**
 * Period to wait to the next iteration when the input interface is closed.
 */
#define IPUT_IFACE_WAIT_USECS (1000* 100)

/**
 * MPEG2 Stream Processors base URL
 */
#define MPEG2_SP_BASE_URL "stream_procs"

/* Debugging purposes only */
//#define SIMULATE_FAIL_DB_UPDATE
#ifndef SIMULATE_FAIL_DB_UPDATE
	#define DB_UPDATE(MPEG2_SP_CTX, LOG_CTX) \
		sp_database_update(MPEG2_SP_CTX, LOG_CTX)
#else
	#define DB_UPDATE(MPEG2_SP_CTX, LOG_CTX) (STAT_ERROR)
#endif

/**
 * Type for processors registering and mapping.
 */
typedef struct proc_reg_s {
	fair_lock_t *lock;
	int proc_id;
	int associated_proc_id;
} proc_reg_t;

typedef struct mpeg2_sp_settings_ctx_s {
	/**
	 * Input URL.
	 * Set to "\0" or NULL to request input stream to be closed.
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
	 * MPEG2 stream processor system identifier (unambiguous string to identify
	 * this stream processor module instance within the whole network).
	 * In the case of the MPEG2 stream processor we use the processor's API
	 * URL as the system Id.
	 */
	char sys_id[1024];

	/* **** ----------------------- Tables/data ----------------------- **** */
	/**
	 * Current copy of the Program Association Table (PAT).
	 */
	psi_table_ctx_t *psi_table_ctx_pat;
	/**
	 * PAT critical section MUTEX.
	 */
	pthread_mutex_t psi_table_ctx_pat_mutex;
	/**
	 * Current copy of the Program Map Table (PMT; composed of a list of
	 * several Program Map Sections -PMSs-).
	 */
	psi_table_ctx_t *psi_table_ctx_pmt;
	/**
	 * PMT critical section MUTEX.
	 */
	pthread_mutex_t psi_table_ctx_pmt_mutex;
	/**
	 * Current copy of the Service Descrition Table (DVB-SDT).
	 */
	psi_table_ctx_t *psi_table_ctx_sdt;
	/**
	 * SDT critical section MUTEX.
	 */
	pthread_mutex_t psi_table_ctx_sdt_mutex;
	/**
	 * PSI tracking/parsing and statistics thread.
	 */
	pthread_t psi_thread;
	/**
	 * Interruptible u-sleep for the PSI tracking and statistics thread.
	 */
	interr_usleep_ctx_t *interr_usleep_ctx_psi_stats;

	/* **** ----------------------- Processors ------------------------ **** */
	/**
	 * PSI processors module context structure.
	 */
	procs_ctx_t *procs_ctx_psi;
	/**
	 * Program processors module context structure.
	 */
	procs_ctx_t *procs_ctx_prog;
	/**
	 * Disassociated program processors module context structure.
	 */
	procs_ctx_t *procs_ctx_dis_prog;

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
	 * Interruptible u-sleep to use in case the input interface is closed by
	 * the calling application.
	 */
	interr_usleep_ctx_t *interr_usleep_ctx_input;
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
		const char *settings_str, const char* href, log_ctx_t *log_ctx,
		va_list arg);
static void mpeg2_sp_close(proc_ctx_t **ref_proc_ctx);
static int mpeg2_sp_rest_put(proc_ctx_t *proc_ctx, const char *str);
static int mpeg2_sp_rest_put2(proc_ctx_t *proc_ctx, const char *str);
static int mpeg2_sp_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse);
static cJSON* mpeg2_sp_rest_get_settings(
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx);
static void mpeg2_sp_rest_get_programs_summary(mpeg2_sp_ctx_t *mpeg2_sp_ctx,
		cJSON *cjson_programs, log_ctx_t *log_ctx);

static int mpeg2_sp_settings_ctx_init(
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx);
static void mpeg2_sp_settings_ctx_deinit(
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx);

static void* distr_thr(void *t);

static void* psi_thr(void *t);
static void compose_pat_and_pmt(mpeg2_sp_ctx_t *mpeg2_sp_ctx,
		log_ctx_t *log_ctx);
static void compose_pmt_pms(mpeg2_sp_ctx_t *mpeg2_sp_ctx, uint16_t pms_pid,
		psi_table_ctx_t *psi_table_ctx_pmt, log_ctx_t *log_ctx);

/**
 * Open a processor instance:
 * - Register processor with given initial settings if desired,
 * - Parse JSON-REST response to get processor Id.
 */
static int procs_post(procs_ctx_t *procs_ctx, const char *proc_name,
		const char *proc_settings, int *ref_proc_id, log_ctx_t *log_ctx);

static int sp_database_update(mpeg2_sp_ctx_t *mpeg2_sp_ctx, log_ctx_t *log_ctx);
static int sp_database_tsk_update(mpeg2_sp_ctx_t *mpeg2_sp_ctx,
		const char *settings_str, log_ctx_t *log_ctx);
static char* mpeg2_sp_rest_get_settings_doc(mpeg2_sp_ctx_t *mpeg2_sp_ctx,
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx);

/* **** Implementations **** */

const proc_if_t proc_if_mpeg2_sp=
{
	"mpeg2_sp", "stream_processor", "n/a",
	(uint64_t)(PROC_FEATURE_BITRATE),
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
		const char *settings_str, const char* href, log_ctx_t *log_ctx,
		va_list arg)
{
	config_t cfg;
	const char *host_ipv4_addr; // Do not release
	int ret_code, end_code= STAT_ERROR, proc_instance_index= -1, proc_id= -1;
	mpeg2_sp_ctx_t *mpeg2_sp_ctx= NULL;
	volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx=
			NULL; // Do not release
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(proc_if!= NULL, return NULL);
	CHECK_DO(settings_str!= NULL, return NULL);
	// Parameter 'href' is allowed to be NULL
	// In this processor, 'log_ctx' *MUST* be NULL as it will be defined
	// internally.
	CHECK_DO(log_ctx== NULL, return NULL); // *MUST* be NULL for this processor

	/* Parse variable arguments */
	proc_instance_index= va_arg(arg, int);
	CHECK_DO(proc_instance_index>= 0, return NULL);

	/* Instantiate the processor's LOG module */
	log_ctx= log_open(proc_instance_index);
	CHECK_DO(log_ctx!= NULL, goto end);
	LOG_CTX_SET(log_ctx);

	/* Allocate context structure */
	mpeg2_sp_ctx= (mpeg2_sp_ctx_t*)calloc(1, sizeof(mpeg2_sp_ctx_t));
	CHECK_DO(mpeg2_sp_ctx!= NULL, goto end);

	/* Register used protocols */
    ret_code= comm_module_opt("COMM_REGISTER_PROTO", &comm_if_udp);
    CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_ECONFLICT, goto end);

	/* **** Special case: ****
	 * First of all we compose the stream processor ID and instantiate the
	 * processor LOG module associating this unambiguous ID.
	 */

	/* MPEG2 stream processor unique System-Id.:
	 * - Initialize configuration file management module;
	 * - Read configuration file;
	 * - Get necessary configuration information (NOTE: All pointers to
	 * configuration resources are static and should not be freed;
	 * configuration file management context should be freed at the end).
	 * Finally we compose the Id. (URL) as follows:
	 * '<host-IPv4-address>/<MPEG2-stream-proc-prefix>/
	 * <MPEG2-stream-proc--index>'
	 */
	config_init(&cfg);
	if(!config_read_file(&cfg, _CONFIG_FILE_DIR))
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
	CHECK_DO(strlen(host_ipv4_addr)+ strlen(MPEG2_SP_BASE_URL)+ sizeof(int)+
			3< sizeof(mpeg2_sp_ctx->sys_id), goto end);
	snprintf(mpeg2_sp_ctx->sys_id, sizeof(mpeg2_sp_ctx->sys_id), "%s/%s/%d",
			host_ipv4_addr, MPEG2_SP_BASE_URL, proc_instance_index);

	/* Instantiate the processor's LOG module */
	((proc_ctx_t*)mpeg2_sp_ctx)->log_ctx= log_ctx;

	/* Get settings structure */
	mpeg2_sp_settings_ctx= &mpeg2_sp_ctx->mpeg2_sp_settings_ctx;

	/* Initialize settings to defaults */
	ret_code= mpeg2_sp_settings_ctx_init(mpeg2_sp_settings_ctx, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Parse and put given settings */
	ret_code= mpeg2_sp_rest_put((proc_ctx_t*)mpeg2_sp_ctx, settings_str);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* **** Initialize rest of context structure **** */

	/* Copy of the PAT */
	mpeg2_sp_ctx->psi_table_ctx_pat= NULL;

	/* PAT critical section MUTEX */
	ret_code= pthread_mutex_init(&mpeg2_sp_ctx->psi_table_ctx_pat_mutex, NULL);
	CHECK_DO(ret_code== 0, goto end);

	/* Copy of the PMT */
	mpeg2_sp_ctx->psi_table_ctx_pmt= NULL;

	/* PMT critical section MUTEX */
	ret_code= pthread_mutex_init(&mpeg2_sp_ctx->psi_table_ctx_pmt_mutex, NULL);
	CHECK_DO(ret_code== 0, goto end);

	/* Copy of the SDT */
	mpeg2_sp_ctx->psi_table_ctx_sdt= NULL;

	/* SDT critical section MUTEX */
	ret_code= pthread_mutex_init(&mpeg2_sp_ctx->psi_table_ctx_sdt_mutex, NULL);
	CHECK_DO(ret_code== 0, goto end);

	/* PSI processors module context structure */
	mpeg2_sp_ctx->procs_ctx_psi= procs_open(LOG_CTX_GET(), TS_MAX_PID_VAL+ 1,
			"psi_processors", mpeg2_sp_ctx->sys_id);
	CHECK_DO(mpeg2_sp_ctx->procs_ctx_psi!= NULL, goto end);

	/* Program processors module context structure */
	mpeg2_sp_ctx->procs_ctx_prog= procs_open(LOG_CTX_GET(), TS_MAX_PID_VAL+ 1,
			"program_processors", mpeg2_sp_ctx->sys_id);
	CHECK_DO(mpeg2_sp_ctx->procs_ctx_prog!= NULL, goto end);

	/* Disassociated program processors module context structure */
	mpeg2_sp_ctx->procs_ctx_dis_prog= procs_open(LOG_CTX_GET(),
			TS_MAX_PID_VAL+ 1, "disassoc_program_processors",
			mpeg2_sp_ctx->sys_id);
	CHECK_DO(mpeg2_sp_ctx->procs_ctx_dis_prog!= NULL, goto end);

	/* Input COMM module instance context structure */
	mpeg2_sp_ctx->comm_ctx_input= NULL; // Set by 'demuxer_opt()'

	/* Input critical section MUTEX */
	ret_code= pthread_mutex_init(&mpeg2_sp_ctx->comm_ctx_input_mutex, NULL);
	CHECK_DO(ret_code== 0, goto end);

	/* Interruptible u-sleep to use in case the input interface is closed */
	mpeg2_sp_ctx->interr_usleep_ctx_input= interr_usleep_open();
	CHECK_DO(mpeg2_sp_ctx->interr_usleep_ctx_input!= NULL, goto end);

	/* Packet distribution thread exit indicator */
	mpeg2_sp_ctx->distr_flag_exit= 0;

	/* Distribution thread */
	//pthread_t distr_thread; //launch threads at the end of this function

	// Reserved for future use: add new fields initializations here...

	/* **** Launch threads and register mandatory PSI processors **** */

	/* Register necessary processors types.
	 * Note that return value 'STAT_ECONFLICT' means that processor was
	 * already registered, so for this case is also O.K.
	 */
	ret_code= procs_module_opt("PROCS_REGISTER_TYPE", &proc_if_psi_table_proc);
	CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_ECONFLICT, goto end);
	ret_code= procs_module_opt("PROCS_REGISTER_TYPE",
			&proc_if_psi_section_proc);
	CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_ECONFLICT, goto end);

	/* Program Association Table (PAT) processor (PID= 0) */
	ret_code= procs_post(mpeg2_sp_ctx->procs_ctx_psi, "psi_table_proc",
			"forced_proc_id=0", &proc_id, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS && proc_id== PSI_PAT_PID_NUMBER, goto end);

	/* Service Description Table (SDT) processor (PID= 17) */
	ret_code= procs_post(mpeg2_sp_ctx->procs_ctx_psi, "psi_table_proc",
			"forced_proc_id=17", &proc_id, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS && proc_id== PSI_DVB_SDT_PID_NUMBER,
			goto end);

	/* **** Finally, launch threads **** */

	/* Launch PSI and statistics thread */
	mpeg2_sp_ctx->interr_usleep_ctx_psi_stats= interr_usleep_open();
	CHECK_DO(mpeg2_sp_ctx->interr_usleep_ctx_psi_stats!= NULL, goto end);
	ret_code= pthread_create(&mpeg2_sp_ctx->psi_thread, NULL, psi_thr,
			(void*)mpeg2_sp_ctx);
	CHECK_DO(ret_code== 0, goto end);

	/* Launch distribution thread */
	ret_code= pthread_create(&mpeg2_sp_ctx->distr_thread, NULL, distr_thr,
			(void*)mpeg2_sp_ctx);
	CHECK_DO(ret_code== 0, goto end);

    end_code= STAT_SUCCESS;
 end:
    if(end_code!= STAT_SUCCESS)
    	mpeg2_sp_close((proc_ctx_t**)&mpeg2_sp_ctx);
	config_destroy(&cfg);
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
	 * - Unblock and close input COMM;
	 * - Unblock (delete/close) all registered/mapped processors;
	 */
	mpeg2_sp_ctx->distr_flag_exit= 1;

	if(mpeg2_sp_ctx->interr_usleep_ctx_input!= NULL)
		interr_usleep_unblock(mpeg2_sp_ctx->interr_usleep_ctx_input);
	comm_close_external(&mpeg2_sp_ctx->comm_ctx_input_mutex,
			&mpeg2_sp_ctx->comm_ctx_input, LOG_CTX_GET());

	for(i= 0; i< (TS_MAX_PID_VAL+ 1); i++) {
		int ret_code;
		if(mpeg2_sp_ctx->procs_ctx_psi!= NULL) {
			ret_code= procs_opt(mpeg2_sp_ctx->procs_ctx_psi,
					"PROCS_ID_DELETE", i);
			ASSERT(ret_code== STAT_SUCCESS || ret_code== STAT_ENOTFOUND);
		}
		if(mpeg2_sp_ctx->procs_ctx_prog!= NULL) {
			ret_code= procs_opt(mpeg2_sp_ctx->procs_ctx_prog,
					"PROCS_ID_DELETE", i);
			ASSERT(ret_code== STAT_SUCCESS || ret_code== STAT_ENOTFOUND);
		}
		if(mpeg2_sp_ctx->procs_ctx_dis_prog!= NULL) {
			ret_code= procs_opt(mpeg2_sp_ctx->procs_ctx_dis_prog,
					"PROCS_ID_DELETE", i);
			ASSERT(ret_code== STAT_SUCCESS || ret_code== STAT_ENOTFOUND);
		}
	}

	LOGV("Waiting for distribution thread to join... "); //comment-me
	pthread_join(mpeg2_sp_ctx->distr_thread, &thread_end_code);
	if(thread_end_code!= NULL) {
		ASSERT(*((int*)thread_end_code)== STAT_SUCCESS);
		free(thread_end_code);
		thread_end_code= NULL;
	}
	LOGV("thread joined O.K.\n"); //comment-me

	/* Join PSI tracking and statistics thread.
	 * - Unblock interruptible-sleep module instance;
	 * - Join the thread.
	 */
	if(mpeg2_sp_ctx->interr_usleep_ctx_psi_stats!= NULL)
		interr_usleep_unblock(mpeg2_sp_ctx->interr_usleep_ctx_psi_stats);
	LOGV("Waiting for PSI/statistics thread to join... "); //comment-me
	pthread_join(mpeg2_sp_ctx->psi_thread, &thread_end_code);
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

	/* Release copy of the PAT */
	psi_table_ctx_release(&mpeg2_sp_ctx->psi_table_ctx_pat);

	/* Release PAT critical section MUTEX */
	ASSERT(pthread_mutex_destroy(&mpeg2_sp_ctx->psi_table_ctx_pat_mutex)== 0);

	/* Release copy of the PMT */
	psi_table_ctx_release(&mpeg2_sp_ctx->psi_table_ctx_pmt);

	/* Release PMT critical section MUTEX */
	ASSERT(pthread_mutex_destroy(&mpeg2_sp_ctx->psi_table_ctx_pmt_mutex)== 0);

	/* Release copy of the SDT */
	psi_table_ctx_release(&mpeg2_sp_ctx->psi_table_ctx_sdt);

	/* Release SDT critical section MUTEX */
	ASSERT(pthread_mutex_destroy(&mpeg2_sp_ctx->psi_table_ctx_sdt_mutex)== 0);

	/* Release interruptible u-sleep module instance */
	interr_usleep_close(&mpeg2_sp_ctx->interr_usleep_ctx_psi_stats);

	/* Release PSI processors module context structure */
	procs_close(&mpeg2_sp_ctx->procs_ctx_psi);

	/* Release program processors module context structure */
	procs_close(&mpeg2_sp_ctx->procs_ctx_prog);

	/* Release disassociated program processors module context structure */
	procs_close(&mpeg2_sp_ctx->procs_ctx_dis_prog);

	/* Release input critical section MUTEX */
	ASSERT(pthread_mutex_destroy(&mpeg2_sp_ctx->comm_ctx_input_mutex)== 0);

	/* Release interruptible u-sleep to use in case the input interface is
	 * closed.
	 */
	interr_usleep_close(&mpeg2_sp_ctx->interr_usleep_ctx_input);

	// Reserved for future use: release other new variables here...

	/* Remember we opened our own LOG module instance... release it */
	log_close(&((proc_ctx_t*)mpeg2_sp_ctx)->log_ctx);

	/* Release context structure */
	free(mpeg2_sp_ctx);
	*ref_proc_ctx= NULL;
}

/**
 * Implements the proc_if_s::rest_put callback.
 * See .proc_if.h for further details.
 * <br>
 * Settings JSON structure may be as follows (can also be a query-string with
 * the same fields):
 * @code
 * {
 *     "tag":string,
 *     "input_url":string,
 *     "flag_clear_logs":boolean,
 *     "flag_purge_disassociated_processors":boolean
 * }
 * @endcode
 */
static int mpeg2_sp_rest_put(proc_ctx_t *proc_ctx, const char *str)
{
	int ret_code, end_code= STAT_ERROR;
	mpeg2_sp_ctx_t *mpeg2_sp_ctx= NULL;
	volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx=
			NULL; // Do not release
	cJSON *cjson_settings_bkp= NULL;
	char *settings_bkp_str= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(str!= NULL, return STAT_ERROR);
	if(strlen(str)== 0)
		return STAT_SUCCESS;

	LOG_CTX_SET(proc_ctx->log_ctx);

	mpeg2_sp_ctx= (mpeg2_sp_ctx_t*)proc_ctx;
	mpeg2_sp_settings_ctx= &mpeg2_sp_ctx->mpeg2_sp_settings_ctx;

	/* Get a copy of current setting in case we fail and need to reestablish */
	cjson_settings_bkp= mpeg2_sp_rest_get_settings(mpeg2_sp_settings_ctx,
			LOG_CTX_GET());
	CHECK_DO(cjson_settings_bkp!= NULL, goto end);

	/* Actually PUT new settings (assume current settings may be modified) */
	ret_code= mpeg2_sp_rest_put2(proc_ctx, str);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Update database with new settings */
#if 0 //FIXME!!
	if(DB_UPDATE(mpeg2_sp_ctx, LOG_CTX_GET())!= STAT_SUCCESS) {
		LOGE("Check point failed.\n");

		/* We have to reestablish backup settings as we failed */
		settings_bkp_str= cJSON_PrintUnformatted(cjson_settings_bkp);
		ASSERT(settings_bkp_str!= NULL);

		ret_code= mpeg2_sp_rest_put2(proc_ctx, settings_bkp_str);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);
		goto end;
	}
#endif

	/* Clear LOGS if applicable */
	if(mpeg2_sp_settings_ctx->flag_clear_logs!= 0) {
		log_clear(LOG_CTX_GET());
		mpeg2_sp_settings_ctx->flag_clear_logs= 0;
	}

	end_code= STAT_SUCCESS;
end:
	if(cjson_settings_bkp!= NULL)
		cJSON_Delete(cjson_settings_bkp);
	if(settings_bkp_str!= NULL)
		free(settings_bkp_str);
	return end_code;
}

static int mpeg2_sp_rest_put2(proc_ctx_t *proc_ctx, const char *str)
{
	int ret_code, end_code= STAT_ERROR;
	mpeg2_sp_ctx_t *mpeg2_sp_ctx= NULL;
	volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx= NULL;
	int flag_is_query= 0; // 0-> JSON / 1->query string
	char* input_url_str= NULL, *tag_str= NULL, *flag_clear_logs_str= NULL,
			*flag_purge_dis_procs_str= NULL;
	cJSON *cjson_settings= NULL;
	cJSON *cjson_aux= NULL; // Do not release
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(str!= NULL, return STAT_ERROR);
	if(strlen(str)== 0)
		return STAT_SUCCESS;

	LOG_CTX_SET(proc_ctx->log_ctx);

	mpeg2_sp_ctx= (mpeg2_sp_ctx_t*)proc_ctx;
	mpeg2_sp_settings_ctx= &mpeg2_sp_ctx->mpeg2_sp_settings_ctx;

	/* Guess settings string representation format (JSON-REST or Query) */
	flag_is_query= (str[0]=='{' && str[strlen(str)-1]=='}')? 0: 1;

	/* Parse JSON or query string and check for new settings */
	if(flag_is_query== 1) {

		/* Input URL */
		input_url_str= uri_parser_query_str_get_value("input_url", str);
		if(input_url_str!= NULL) {
			ret_code= comm_reset_external(&mpeg2_sp_ctx->comm_ctx_input_mutex,
					input_url_str, NULL, COMM_MODE_IPUT, LOG_CTX_GET(),
					&mpeg2_sp_ctx->comm_ctx_input);
			if(ret_code== STAT_SUCCESS) {
				if(mpeg2_sp_settings_ctx->input_url!= NULL)
					free(mpeg2_sp_settings_ctx->input_url);
				mpeg2_sp_settings_ctx->input_url= input_url_str;
				input_url_str= NULL; // Avoid double referencing
			} else {
				end_code= ret_code;
				goto end;
			}
		}

		/* DEMUXER tag */
		tag_str= uri_parser_query_str_get_value("tag", str);
		if(tag_str!= NULL) {
			if(mpeg2_sp_settings_ctx->tag!= NULL)
				free(mpeg2_sp_settings_ctx->tag);
			mpeg2_sp_settings_ctx->tag= tag_str;
			tag_str= NULL; // Avoid double referencing
		}

		/* Flag indicating to clear logs */
		flag_clear_logs_str= uri_parser_query_str_get_value("flag_clear_logs",
				str);
		if(flag_clear_logs_str!= NULL)
			mpeg2_sp_settings_ctx->flag_clear_logs= (strncmp(
					flag_clear_logs_str, "true", strlen("true"))== 0)? 1: 0;

		/* Flag to purge disassociated processors */
		flag_purge_dis_procs_str= uri_parser_query_str_get_value(
				"flag_purge_disassociated_processors", str);
		if(flag_purge_dis_procs_str!= NULL)
			mpeg2_sp_settings_ctx->flag_purge_disassociated_processors=
					(strncmp(flag_purge_dis_procs_str, "true", strlen("true"))
							== 0)? 1: 0;

	} else {
		/* In the case string format is JSON-REST, parse to cJSON structure */
		cjson_settings= cJSON_Parse(str);
		CHECK_DO(cjson_settings!= NULL, goto end);

		/* Input URL */
		cjson_aux= cJSON_GetObjectItem(cjson_settings, "input_url");
		if(cjson_aux!= NULL && cjson_aux->valuestring!= NULL) {
			input_url_str= strdup(cjson_aux->valuestring);
			CHECK_DO(input_url_str!= NULL, goto end);
			ret_code= comm_reset_external(&mpeg2_sp_ctx->comm_ctx_input_mutex,
					input_url_str, NULL, COMM_MODE_IPUT, LOG_CTX_GET(),
					&mpeg2_sp_ctx->comm_ctx_input);
			if(ret_code== STAT_SUCCESS) {
				if(mpeg2_sp_settings_ctx->input_url!= NULL)
					free(mpeg2_sp_settings_ctx->input_url);
				mpeg2_sp_settings_ctx->input_url= input_url_str;
				input_url_str= NULL; // Avoid double referencing
			} else {
				end_code= ret_code;
				goto end;
			}
		}

		/* DEMUXER tag */
		cjson_aux= cJSON_GetObjectItem(cjson_settings, "tag");
		if(cjson_aux!= NULL && cjson_aux->valuestring!= NULL) {
			tag_str= strdup(cjson_aux->valuestring);
			CHECK_DO(tag_str!= NULL, goto end);
			if(mpeg2_sp_settings_ctx->tag!= NULL)
				free(mpeg2_sp_settings_ctx->tag);
			mpeg2_sp_settings_ctx->tag= tag_str;
			tag_str= NULL; // Avoid double referencing
		}

		/* Flag indicating to clear logs */
		cjson_aux= cJSON_GetObjectItem(cjson_settings, "flag_clear_logs");
		if(cjson_aux!= NULL)
			mpeg2_sp_settings_ctx->flag_clear_logs=
					(cjson_aux->type==cJSON_True)?1 : 0;

		/* Flag to purge disassociated processors */
		cjson_aux= cJSON_GetObjectItem(cjson_settings,
				"flag_purge_disassociated_processors_str");
		if(cjson_aux!= NULL)
			mpeg2_sp_settings_ctx->flag_purge_disassociated_processors=
					(cjson_aux->type==cJSON_True)?1 : 0;
	}

	// Reserved for future use

	end_code= STAT_SUCCESS;
end:
	if(input_url_str!= NULL)
		free(input_url_str);
	if(tag_str!= NULL)
		free(tag_str);
	if(flag_clear_logs_str!= NULL)
		free(flag_clear_logs_str);
	if(flag_purge_dis_procs_str!= NULL)
		free(flag_purge_dis_procs_str);
	if(cjson_settings!= NULL)
		cJSON_Delete(cjson_settings);
	return end_code;
}

/**
 * Implements the proc_if_s::rest_get callback.
 * See .proc_if.h for further details.
 *
 * JSON string to be returned:
 * @code
 * {
 *     "sys_id":string,
 *     "input_bitrate":number, -kbps-
 *     "log_traces":
 *     [
 *         {
 *             "log_trace_code":string,
 *             "log_trace_desc":string,
 *             "log_trace_date":string,
 *             “log_trace_counter”:number
 *         },
 *         {...}
 *     ],
 *     "settings":
 *     {
 *         ...
 *     },
 *     "programs":
 *     [
 *         {
 *             "program_number":number,
 *             "service_name":string,
 *             "isBeingProcessed":boolean,
 *             "links":
 *             [
 *                 {"rel":	"self", "href":string}
 *             ]
 *         },
 *         ....
 *     ],
 *     "program_processors": [],
 *     “links”:
 *     [
 *         {"rel":"self", "href":string}
 *     ]
 * }
 * @endcode
 *
 * Notes on fields:
 * - "hasBeenDisassociated": Refers to a program associated to a previous
 * version of the PAT and PMT, but no longer associated in current tables.
 * It is preserved only because is still being processed.
 */
static int mpeg2_sp_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse)
{
	llist_t *n;
	int ret_code, end_code= STAT_ERROR;
	mpeg2_sp_ctx_t *mpeg2_sp_ctx= NULL;
	volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx= NULL;
	cJSON *cjson_rest= NULL;
	cJSON *cjson_aux= NULL, *cjson_log_traces= NULL, *cjson_programs= NULL,
			*cjson_links= NULL, *cjson_link= NULL; // Do not release
	char href[128+ 5]= {0};
	llist_t *log_line_ctx_llist= NULL;
	char *procs_rest_str= NULL;
	cJSON *cjson_procs_rest= NULL, *cjson_procs_array= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(rest_fmt< PROC_IF_REST_FMT_ENUM_MAX, return STAT_ERROR);
	CHECK_DO(ref_reponse!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	mpeg2_sp_ctx= (mpeg2_sp_ctx_t*)proc_ctx;
	mpeg2_sp_settings_ctx= &mpeg2_sp_ctx->mpeg2_sp_settings_ctx;

	*ref_reponse= NULL;

	/* Create cJSON tree root object */
	cjson_rest= cJSON_CreateObject();
	CHECK_DO(cjson_rest!= NULL, goto end);

	cjson_aux= cJSON_CreateString(mpeg2_sp_ctx->sys_id);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "id_str", cjson_aux);

	/* Bitrate data (do not use MUTEX/critical section for bitrate) */
	cjson_aux= cJSON_CreateNumber((double)proc_ctx->bitrate[PROC_IPUT]);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "input_bitrate", cjson_aux);

	cjson_log_traces= cJSON_CreateArray();
	CHECK_DO(cjson_log_traces!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "log_traces", cjson_log_traces);

	/* Add settings elements */
	cjson_aux= mpeg2_sp_rest_get_settings(mpeg2_sp_settings_ctx, LOG_CTX_GET());
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "settings", cjson_aux);

	cjson_programs= cJSON_CreateArray();
	CHECK_DO(cjson_programs!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "programs", cjson_programs);

	/* Add program-processors information */
	ret_code= procs_opt(mpeg2_sp_ctx->procs_ctx_prog, "PROCS_GET",
			&procs_rest_str, NULL);
	CHECK_DO(ret_code== STAT_SUCCESS && procs_rest_str!= NULL, goto end);
	cjson_procs_rest= cJSON_Parse(procs_rest_str);
	CHECK_DO(cjson_procs_rest!= NULL, goto end);
	cjson_procs_array= cJSON_DetachItemFromObject(cjson_procs_rest,
			"program_processors");
	CHECK_DO(cjson_procs_array!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "program_processors", cjson_procs_array);
	cjson_procs_array= NULL; // Avoid double referencing

	/* Links */
	cjson_links= cJSON_CreateArray();
	CHECK_DO(cjson_links!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "links", cjson_links);

	cjson_link= cJSON_CreateObject();
	CHECK_DO(cjson_link!= NULL, goto end);
	cJSON_AddItemToArray(cjson_links, cjson_link);

	cjson_aux= cJSON_CreateString("self");
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_link, "rel", cjson_aux);

	snprintf(href, sizeof(href), "%s.json", mpeg2_sp_ctx->sys_id);
	cjson_aux= cJSON_CreateString(href);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_link, "href", cjson_aux);

	/* **** Update LOG-traces array **** */
	log_line_ctx_llist= (llist_t*)
			log_get(LOG_CTX_GET()); //Get copy (NULL is allowed)
	for(n= log_line_ctx_llist; n!= NULL; n= n->next) {
		log_line_ctx_t *log_line_ctx_nth;
		cJSON *cjson_log_trace= NULL;

		log_line_ctx_nth= (log_line_ctx_t*)n->data;
		CHECK_DO(log_line_ctx_nth!= NULL, continue);

		cjson_log_trace= cJSON_CreateObject();
		CHECK_DO(cjson_log_trace!= NULL, goto end);
		cJSON_AddItemToArray(cjson_log_traces, cjson_log_trace);

		cjson_aux= cJSON_CreateString(log_line_ctx_nth->code);
		CHECK_DO(cjson_aux!= NULL, goto end);
		cJSON_AddItemToObject(cjson_log_trace, "log_trace_code", cjson_aux);

		cjson_aux= cJSON_CreateString(log_line_ctx_nth->desc);
		CHECK_DO(cjson_aux!= NULL, goto end);
		cJSON_AddItemToObject(cjson_log_trace, "log_trace_desc", cjson_aux);

		cjson_aux= cJSON_CreateString(log_line_ctx_nth->date);
		CHECK_DO(cjson_aux!= NULL, goto end);
		cJSON_AddItemToObject(cjson_log_trace, "log_trace_date", cjson_aux);

		cjson_aux= cJSON_CreateNumber((double)log_line_ctx_nth->count);
		CHECK_DO(cjson_aux!= NULL, goto end);
		cJSON_AddItemToObject(cjson_log_trace, "log_trace_counter",
				cjson_aux);
	}

	/* **** Add program information to the representational state **** */
	mpeg2_sp_rest_get_programs_summary(mpeg2_sp_ctx, cjson_programs,
			LOG_CTX_GET());

	// Reserved for future use: set other data values here...

	/* Format response to be returned */
	switch(rest_fmt) {
	case PROC_IF_REST_FMT_CHAR:
		/* Print cJSON structure data to char string */
		*ref_reponse= (void*)CJSON_PRINT(cjson_rest);
		CHECK_DO(*ref_reponse!= NULL && strlen((char*)*ref_reponse)> 0,
				goto end);
		break;
	case PROC_IF_REST_FMT_CJSON:
		*ref_reponse= (void*)cjson_rest;
		cjson_rest= NULL; // Avoid double referencing
		break;
	default:
		LOGE("Unknown format requested for processor REST\n");
		goto end;
	}

	end_code= STAT_SUCCESS;
end:
	if(cjson_rest!= NULL)
		cJSON_Delete(cjson_rest);
	LLIST_RELEASE(&log_line_ctx_llist, log_line_ctx_release, log_line_ctx_t);
	if(procs_rest_str!= NULL)
		free(procs_rest_str);
	if(cjson_procs_rest!= NULL)
		cJSON_Delete(cjson_procs_rest);
	if(cjson_procs_array!= NULL)
		cJSON_Delete(cjson_procs_array);
	return end_code;
}

/**
 * Get MPEG2 Stream Processor settings REST.
 * Settings JSON structure is as follows:
 * @code
 * {
 *     "tag":string,
 *     "input_url":string,
 *     "flag_clear_logs":boolean,
 *     "flag_purge_disassociated_processors":boolean
 * }
 * @endcode
 */
static cJSON* mpeg2_sp_rest_get_settings(
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx)
{
	int flag_clear_logs, flag_purge_disassociated_processors;
	char *tag;
	char *input_url;
	int end_code= STAT_ERROR;
	cJSON *cjson_settings= NULL;
	cJSON *cjson_aux= NULL; // Do not release
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(mpeg2_sp_settings_ctx!= NULL, return NULL);

	/* Settings object */
	cjson_settings= cJSON_CreateObject();
	CHECK_DO(cjson_settings!= NULL, goto end);

	/* **** Add settings elements **** */

	tag= mpeg2_sp_settings_ctx->tag;
	if(tag== NULL)
		tag= "";
	cjson_aux= cJSON_CreateString(tag);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_settings, "tag", cjson_aux);

	/* Input URL */
	input_url= mpeg2_sp_settings_ctx->input_url;
	if(input_url== NULL)
		input_url= "";
	cjson_aux= cJSON_CreateString(input_url);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_settings, "input_url", cjson_aux);

	/* Flag to clear log register */
	flag_clear_logs= mpeg2_sp_settings_ctx->flag_clear_logs;
	cjson_aux= cJSON_CreateBool(flag_clear_logs);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_settings, "flag_clear_logs", cjson_aux);

	/* Flag to purge disassociated processors */
	flag_purge_disassociated_processors=
			mpeg2_sp_settings_ctx->flag_purge_disassociated_processors;
	cjson_aux= cJSON_CreateBool(flag_purge_disassociated_processors);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_settings,
			"flag_purge_disassociated_processors", cjson_aux);

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS) {
		if(cjson_settings!= NULL) {
			cJSON_Delete(cjson_settings);
			cjson_settings= NULL;
		}
	}
	return cjson_settings;
}

/**
 * Update program REST summary array (reduced information):
 * @code
 * [
 *     {
 *         "program_number":number,
 *         "service_name":string,
 *         "processor_associated":boolean,
 *         "links":
 *         [
 *             {"rel":	"self", "href":string}
 *         ]
 *     },
 *     ...
 * }
 * @endcode
 */
static void mpeg2_sp_rest_get_programs_summary(mpeg2_sp_ctx_t *mpeg2_sp_ctx,
		cJSON *cjson_programs, log_ctx_t *log_ctx)
{
	llist_t *n;
	LOG_CTX_INIT(log_ctx);
	cJSON *cjson_program= NULL;
	psi_table_ctx_t *psi_table_ctx_pat= NULL,
			*psi_table_ctx_sdt= NULL; // Do not release
	char href[256]= {0};

	/* Check arguments */
	CHECK_DO(mpeg2_sp_ctx!= NULL, return);
	CHECK_DO(cjson_programs!= NULL, return);
	// argument 'log_ctx' is allowed to be NULL

	/* Lock PAT register critical section; fetch registered PAT */
	ASSERT(pthread_mutex_lock(&mpeg2_sp_ctx->psi_table_ctx_pat_mutex)== 0);
	psi_table_ctx_pat= mpeg2_sp_ctx->psi_table_ctx_pat;
	if(psi_table_ctx_pat== NULL)
		goto end; // PAT still not parsed

	/* Iterate over PAT sections to get program list */
	for(n= psi_table_ctx_pat->psi_section_ctx_llist; n!= NULL; n= n->next) {
		psi_section_ctx_t *psi_section_ctx;
		psi_pas_ctx_t *psi_pas_ctx;
		llist_t *n2;

		psi_section_ctx= (psi_section_ctx_t*)n->data;
		CHECK_DO(psi_section_ctx!= NULL, continue);
		psi_pas_ctx= (psi_pas_ctx_t*)psi_section_ctx->data;
		CHECK_DO(psi_pas_ctx!= NULL, continue);

		/* Iterate over PAS programs */
		for(n2= psi_pas_ctx->psi_pas_prog_ctx_llist; n2!= NULL; n2= n2->next) {
			int ret_code;
			psi_pas_prog_ctx_t *psi_pas_prog_ctx;
			uint16_t program_number, prog_pid;
			cJSON *cjson_aux= NULL, *cjson_links= NULL,
					*cjson_link; // Do not release
			const char *service_name= NULL; // Do not release
			char *rest_str= NULL;

			psi_pas_prog_ctx= (psi_pas_prog_ctx_t*)n2->data;
			CHECK_DO(psi_section_ctx!= NULL, continue);

			if((program_number= psi_pas_prog_ctx->program_number)== 0)
				continue; // Skip 'network_PID' value if present
			prog_pid= psi_pas_prog_ctx->reference_pid;

			if(cjson_program!= NULL) {
				cJSON_Delete(cjson_program);
				cjson_program= NULL;
			}
			cjson_program= cJSON_CreateObject();
			CHECK_DO(cjson_program!= NULL, goto end);
			//cJSON_AddItemToArray(..., cjson_program); // at the end of loop

			cjson_aux= cJSON_CreateNumber((double)program_number);
			CHECK_DO(cjson_aux!= NULL, goto end);
			cJSON_AddItemToObject(cjson_program, "program_number", cjson_aux);

			if(psi_table_ctx_sdt!= NULL) {
				service_name=
					psi_table_dvb_sdt_ctx_filter_service_name_by_program_num(
							psi_table_ctx_sdt, program_number);
			}
			if(service_name== NULL)
				service_name= "";
			cjson_aux= cJSON_CreateString(service_name);
			CHECK_DO(cjson_aux!= NULL, goto end);
			cJSON_AddItemToObject(cjson_program, "service_name", cjson_aux);

			/* Check if there is a processor associated */
			ret_code= procs_opt(mpeg2_sp_ctx->procs_ctx_prog, "PROCS_ID_GET",
					prog_pid, &rest_str);
			if(rest_str!= NULL)
				free(rest_str);
			if(ret_code== STAT_SUCCESS)
				cjson_aux= cJSON_CreateTrue();
			else
				cjson_aux= cJSON_CreateFalse();
			CHECK_DO(cjson_aux!= NULL, goto end);
			cJSON_AddItemToObject(cjson_program, "processor_associated",
					cjson_aux);

			/* Program links */
			cjson_links= cJSON_CreateArray();
			CHECK_DO(cjson_links!= NULL, goto end);
			cJSON_AddItemToObject(cjson_program, "links", cjson_links);

			cjson_link= cJSON_CreateObject();
			CHECK_DO(cjson_link!= NULL, goto end);
			cJSON_AddItemToArray(cjson_links, cjson_link);

			cjson_aux= cJSON_CreateString("self");
			CHECK_DO(cjson_aux!= NULL, goto end);
			cJSON_AddItemToObject(cjson_link, "rel", cjson_aux);

			snprintf(href, sizeof(href), "%s/programs/%u.json",
					mpeg2_sp_ctx->sys_id, prog_pid);
			cjson_aux= cJSON_CreateString(href);
			CHECK_DO(cjson_aux!= NULL, goto end);
			cJSON_AddItemToObject(cjson_link, "href", cjson_aux);

			/* Finally, attach program information */
			cJSON_AddItemToArray(cjson_programs, cjson_program);
			cjson_program= NULL;
		}
	}

end:
	ASSERT(pthread_mutex_unlock(&mpeg2_sp_ctx->psi_table_ctx_pat_mutex)== 0);
	if(cjson_program!= NULL)
		cJSON_Delete(cjson_program);
	return;
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
		ret_code= comm_recv_external(&mpeg2_sp_ctx->comm_ctx_input_mutex,
				&mpeg2_sp_ctx->comm_ctx_input, (void**)&recv_buf,
				&recv_buf_size, NULL, NULL, LOG_CTX_GET());
		if(ret_code!= STAT_SUCCESS) {
			if(ret_code== STAT_ENODATA) {
				/* Sleep a little longer -interruptible- */
				ret_code= interr_usleep(mpeg2_sp_ctx->interr_usleep_ctx_input,
						IPUT_IFACE_WAIT_USECS);
				ASSERT(ret_code!= STAT_ERROR);
				continue;
			}
			if(ret_code!= STAT_EOF && ret_code!= STAT_ETIMEDOUT)
				LOGE("Input interface (MPEG2-TS Stream Processor Id. %d) "
						"failed to receive new packet\n",
						((proc_ctx_t*)mpeg2_sp_ctx)->proc_instance_index);
			schedule(); // schedule to avoid closed loops
			continue;
		}
		CHECK_DO(recv_buf!= NULL, schedule(); continue);

		for(pkt_p= recv_buf; recv_buf_size>= TS_PKT_SIZE;
				pkt_p+= TS_PKT_SIZE, recv_buf_size-= TS_PKT_SIZE) {

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

			/* Try sending new data packets to assigned processor if any */
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
			proc_frame_ctx.es_id= pid; // pass PID as stream ID!.
			ret_code= procs_send_frame(mpeg2_sp_ctx->procs_ctx_psi, pid,
					&proc_frame_ctx);
			ASSERT(ret_code!= STAT_ERROR);
			ret_code= procs_send_frame(mpeg2_sp_ctx->procs_ctx_prog, pid,
					&proc_frame_ctx);
			ASSERT(ret_code!= STAT_ERROR);
			ret_code= procs_send_frame(mpeg2_sp_ctx->procs_ctx_dis_prog, pid,
					&proc_frame_ctx);
			ASSERT(ret_code!= STAT_ERROR);
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
		/* Check sanity of received packet size */
		if(recv_buf_size!= 0) {
			LOGE("Stream processor (Id. %d) received a corrupted TS input "
					"packet: erroneous packet size (%d bytes). PID= %u "
					"(0x%0x)\n",
					((proc_ctx_t*)mpeg2_sp_ctx)->proc_instance_index,
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

/**
 * Periodic thread to keep track and updated representational state of the
 * input MPEG2-TS PSI tables.
 */
static void* psi_thr(void *t)
{
	mpeg2_sp_ctx_t *mpeg2_sp_ctx= (mpeg2_sp_ctx_t*)t; // Do not release
	proc_ctx_t *proc_ctx= NULL; // Do not release (alias)
	int *ref_end_code= NULL; // Do not release
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

		/* Get PAT and PMT.
		 * This function is also responsible for launching PMT processors if
		 * applicable (PMT processors are in charge of parsing and generating
		 * the Program Map Sections -PMS's-).
		 */
		compose_pat_and_pmt(mpeg2_sp_ctx, LOG_CTX_GET());

		/* Periodic-sleep */
		ret_code= interr_usleep(mpeg2_sp_ctx->interr_usleep_ctx_psi_stats,
				PSI_THREAD_PERIOD_USECS);
		ASSERT(ret_code!= STAT_ERROR);
	}

	*ref_end_code= STAT_SUCCESS;
end:
	return (void*)ref_end_code;
}

static void compose_pat_and_pmt(mpeg2_sp_ctx_t *mpeg2_sp_ctx,
		log_ctx_t *log_ctx)
{
	llist_t *n;
	int ret_code;
	psi_table_ctx_t *psi_table_ctx_pat= NULL, *psi_table_ctx_pmt= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check argument */
	CHECK_DO(mpeg2_sp_ctx!= NULL, return);

	/* Allocate empty PMT (substitutive candidate to current registered PMT) */
	psi_table_ctx_pmt= psi_table_ctx_allocate();
	CHECK_DO(psi_table_ctx_pmt!= NULL, goto end);

	/* Get current Program Association Table (PAT) */
	ret_code= procs_opt(mpeg2_sp_ctx->procs_ctx_psi,
			"PROCS_ID_GET_CSTRUCT_REST", PSI_PAT_PID_NUMBER,
			&psi_table_ctx_pat);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);
	if(psi_table_ctx_pat== NULL)
		goto end; // We may not still have parsed a PAT table

	/* Iterate over PAT sections to get program numbers */
	for(n= psi_table_ctx_pat->psi_section_ctx_llist; n!= NULL; n= n->next) {
		llist_t *n2;
		psi_section_ctx_t *psi_section_ctx_ith;
		psi_pas_ctx_t *psi_pas_ctx_ith;

		psi_section_ctx_ith= (psi_section_ctx_t*)n->data;
		CHECK_DO(psi_section_ctx_ith!= NULL, continue);

		psi_pas_ctx_ith= psi_section_ctx_ith->data;
		CHECK_DO(psi_pas_ctx_ith!= NULL, continue);

		/* Iterate over each program */
		for(n2= psi_pas_ctx_ith->psi_pas_prog_ctx_llist; n2!= NULL;
				n2= n2->next) {
			psi_pas_prog_ctx_t *psi_pas_prog_ctx_jth;

			psi_pas_prog_ctx_jth= (psi_pas_prog_ctx_t*)n2->data;
			CHECK_DO(psi_pas_prog_ctx_jth!= NULL, continue);

			/* Note that for program number= 0, 'reference_pid'== network PID;
			 * we skip this case.
			 */
			if(psi_pas_prog_ctx_jth->program_number== 0)
				continue;
			compose_pmt_pms(mpeg2_sp_ctx, psi_pas_prog_ctx_jth->reference_pid,
					psi_table_ctx_pmt, LOG_CTX_GET());
		}
	}

	/* Finally, update PAT and PMT register with the tables just parsed */
	ASSERT(pthread_mutex_lock(&mpeg2_sp_ctx->psi_table_ctx_pat_mutex)== 0);
	if(mpeg2_sp_ctx->psi_table_ctx_pat!= NULL)
		psi_table_ctx_release(&mpeg2_sp_ctx->psi_table_ctx_pat);
	mpeg2_sp_ctx->psi_table_ctx_pat= psi_table_ctx_pat;
	ASSERT(pthread_mutex_unlock(&mpeg2_sp_ctx->psi_table_ctx_pat_mutex)== 0);
	psi_table_ctx_pat= NULL; // Avoid double referencing

	ASSERT(pthread_mutex_lock(&mpeg2_sp_ctx->psi_table_ctx_pmt_mutex)== 0);
	if(mpeg2_sp_ctx->psi_table_ctx_pmt!= NULL)
		psi_table_ctx_release(&mpeg2_sp_ctx->psi_table_ctx_pmt);
	mpeg2_sp_ctx->psi_table_ctx_pmt= psi_table_ctx_pmt;
	ASSERT(pthread_mutex_unlock(&mpeg2_sp_ctx->psi_table_ctx_pmt_mutex)== 0);
	psi_table_ctx_pmt= NULL; // Avoid double referencing

end:
	if(psi_table_ctx_pat!= NULL)
		psi_table_ctx_release(&psi_table_ctx_pat);
	if(psi_table_ctx_pmt!= NULL)
		psi_table_ctx_release(&psi_table_ctx_pmt);
	return;
}

static void compose_pmt_pms(mpeg2_sp_ctx_t *mpeg2_sp_ctx, uint16_t pms_pid,
		psi_table_ctx_t *psi_table_ctx_pmt, log_ctx_t *log_ctx)
{
	int ret_code;
	psi_section_ctx_t *psi_section_ctx_pms= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(mpeg2_sp_ctx!= NULL, return);
	CHECK_DO(pms_pid< TS_MAX_PID_VAL, return);
	CHECK_DO(psi_table_ctx_pmt!= NULL, return);

	/* Check if PMS parser/processor is already launched.
	 * If it is the case, we should be able to get corresponding PMS data.
	 * Otherwise, we launch a new PMS processor and register it.
	 */
	ret_code= procs_opt(mpeg2_sp_ctx->procs_ctx_psi,
			"PROCS_ID_GET_CSTRUCT_REST", pms_pid, &psi_section_ctx_pms);
	if(ret_code== STAT_SUCCESS) {
		// Note that 'ret_code' can be success but not have PMS yet ...
		if(psi_section_ctx_pms!= NULL) {
			/* Add PMS to PMT composition */
			ret_code= llist_push(&psi_table_ctx_pmt->psi_section_ctx_llist,
					psi_section_ctx_pms);
			CHECK_DO(ret_code== STAT_SUCCESS, goto end);
			psi_section_ctx_pms= NULL; // Avoid double referencing
		}
	} else {
		int proc_id= -1;
		char settings[32]= {0}; // reserve long enough array

		/* Launch PMS PSI-processor */
		LOGV("Launching PMS PSI-processor %u\n", pms_pid); // comment-me
		snprintf(settings, sizeof(settings), "forced_proc_id=%u", pms_pid);
		ret_code= procs_post(mpeg2_sp_ctx->procs_ctx_psi, "psi_section_proc",
				settings, &proc_id, LOG_CTX_GET());
		CHECK_DO(ret_code== STAT_SUCCESS && proc_id== pms_pid, goto end);
	}

end:
	if(psi_section_ctx_pms!= NULL)
		psi_section_ctx_release(&psi_section_ctx_pms);
	return;
}

static int procs_post(procs_ctx_t *procs_ctx, const char *proc_name,
		const char *proc_settings, int *ref_proc_id, log_ctx_t *log_ctx)
{
	int ret_code, end_code= STAT_ERROR, proc_id= -1;
	char *rest_str= NULL;
	cJSON *cjson_rest= NULL, *cjson_aux= NULL;
	LOG_CTX_INIT(log_ctx);

	ret_code= procs_opt(procs_ctx, "PROCS_POST", proc_name, proc_settings,
			&rest_str);
	CHECK_DO(ret_code== STAT_SUCCESS && rest_str!= NULL, goto end);

	cjson_rest= cJSON_Parse(rest_str);
	CHECK_DO(cjson_rest!= NULL, goto end);

	cjson_aux= cJSON_GetObjectItem(cjson_rest, "proc_id");
	CHECK_DO(cjson_aux!= NULL, goto end);

	proc_id= cjson_aux->valuedouble;
	CHECK_DO(proc_id>= 0, goto end);

	 *ref_proc_id= proc_id;
	 end_code= STAT_SUCCESS;
end:
	if(rest_str!= NULL)
		free(rest_str);
	if(cjson_rest!= NULL)
		cJSON_Delete(cjson_rest);
	return end_code;
}

/**
 * Update database entry corresponding to this stream-processor document.
 */
static int sp_database_update(mpeg2_sp_ctx_t *mpeg2_sp_ctx, log_ctx_t *log_ctx)
{
	int ret_code, end_code= STAT_ERROR;
	char *settings_str= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(mpeg2_sp_ctx!= NULL, return STAT_ERROR);

	/* Get settings cJSON structure */
	settings_str= mpeg2_sp_rest_get_settings_doc(mpeg2_sp_ctx,
			&mpeg2_sp_ctx->mpeg2_sp_settings_ctx, LOG_CTX_GET());
	CHECK_DO(settings_str!= NULL, goto end);
	LOGV("Settings-doc.: '%s'\n", settings_str);//comment-me

	/* System perform system call to update database */
	ret_code= sp_database_tsk_update(mpeg2_sp_ctx, settings_str, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
	if(settings_str!= NULL)
		free(settings_str);
	return end_code;
}

static int sp_database_tsk_update(mpeg2_sp_ctx_t *mpeg2_sp_ctx,
		const char *settings_str, log_ctx_t *log_ctx)
{
	int ret_code;
	pid_t w, child_pid= 0; // process ID
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(mpeg2_sp_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(settings_str!= NULL && strlen(settings_str)> 0, return STAT_ERROR);

	/* Fork process */
	child_pid= fork();
	if(child_pid< 0) {
		LOGE("Could not fork program process (Id. %d)\n",
				((proc_ctx_t*)mpeg2_sp_ctx)->proc_instance_index);
		return STAT_ERROR;
	} else if(child_pid== 0) {
		/* **** CHILD CODE ('child_pid'== 0) **** */
		char *const args[] = {_INSTALL_DIR"/bin/dbdriver_apps_procs_docs",
				(char *const)settings_str, NULL};
		char *const envs[] = {"LD_LIBRARY_PATH="_INSTALL_DIR"/lib", NULL};
		if((ret_code= execve(_INSTALL_DIR"/bin/dbdriver_apps_procs_docs",
				args, envs))< 0) { // execve won't return if succeeded
			exit(ret_code);
		}
	}

	/* **** PARENT CODE **** */

	LOGV("Waiting database task to terminate... \n"); // comment-me
    w= waitpid(child_pid, &ret_code, WUNTRACED);
    ASSERT(w>= 0);
    if(w>= 0) {
        if(WIFEXITED(ret_code)) {
        	LOGV("task ended (code= %d)\n",
        			WEXITSTATUS(ret_code)); //comment-me
        	return (ret_code== EXIT_SUCCESS)? STAT_SUCCESS: STAT_ERROR;
        }
#if 0 //comment-me
        else if(WIFSIGNALED(ret_code)) {
        	LOGV("task killed by signal %d\n", WTERMSIG(ret_code));
        } else if(WIFSTOPPED(ret_code)) {
        	LOGV("task stopped by signal %d\n", WSTOPSIG(ret_code));
        } else if(WIFCONTINUED(ret_code)) {
        	LOGV("task continued\n");
        }
#endif
    }
    LOGV("... waited returned K.O.\n"); // comment-me
	return STAT_ERROR;
}

/**
 * Get MPEG2 Stream Processor settings REST but adding the system-Id.;
 * this new representation is used as the stream processor's document in the
 * system configuration database.
 * The modified settings JSON structure is as follows:
 * @code
 * {
 *     ... // old settings
 *     "sys_id":string
 * }
 * @endcode
 */
static char* mpeg2_sp_rest_get_settings_doc(mpeg2_sp_ctx_t *mpeg2_sp_ctx,
		volatile mpeg2_sp_settings_ctx_t *mpeg2_sp_settings_ctx,
		log_ctx_t *log_ctx)
{
	int end_code= STAT_ERROR;
	cJSON *cjson_settings= NULL;
	cJSON *cjson_aux= NULL; // Do not release
	char *response= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(mpeg2_sp_ctx!= NULL, return NULL);
	CHECK_DO(mpeg2_sp_settings_ctx!= NULL, return NULL);

	/* Get settings cJSON structure */
	cjson_settings= mpeg2_sp_rest_get_settings(
			&mpeg2_sp_ctx->mpeg2_sp_settings_ctx, LOG_CTX_GET());
	CHECK_DO(cjson_settings!= NULL, goto end);

	/* Add 'system identifier' to the settings elements */
	cjson_aux= cJSON_CreateString(mpeg2_sp_ctx->sys_id);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_settings, "sys_id", cjson_aux);

	/* Print cJSON structure data to char string */
	response= CJSON_PRINT(cjson_settings);
	CHECK_DO(response!= NULL && strlen(response)> 0, goto end);

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS) {
		if(response!= NULL)
			free(response);
	}
	if(cjson_settings!= NULL) {
		cJSON_Delete(cjson_settings);
		cjson_settings= NULL;
	}
	return response;
}
