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
 * @file app_prog_proc.c
 * @brief MPEG2-SPTS/MPTS program processor heavy weight process.
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

#include <libcjson/cJSON.h>
#include <libmbedtls_base64/base64.h>

#define ENABLE_DEBUG_LOGS //uncomment to trace logs
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/uri_parser.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocsutils/llist.h>
#include <libmediaprocs/procs.h>
#include <libmediaprocs/procs_api_http.h>
#include <libmediaprocs/proc.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocscodecs/bypass.h>
#include "../src/ts.h"
#include "../src/psi.h"
//#include <../src/psi_enc.h //FIXME!!
#include "../src/psi_dec.h"
#include "../src/psi_table.h"
//#include <../src/psi_table_enc.h> //FIXME!!
#include "../src/psi_table_dec.h"
#include "../src/psi_desc.h"

/* **** Definitions **** */

/* Debugging purposes only */
//#define SIMULATE_FAIL_DB_UPDATE
#ifndef SIMULATE_FAIL_DB_UPDATE
	#define SIMULATE_CRASH()
#else
	#define SIMULATE_CRASH() exit(EXIT_FAILURE)
#endif

#define STR_JSON_REST 	0
#define STR_URL_QUERY 	1

/**
 * Program-processor module instance bit-rate control type indicator.
 */
typedef enum {
	PROG_PROC_BRCTRL_NONE= 0,
	PROG_PROC_BRCTRL_CBR,
	PROG_PROC_BRCTRL_VBR_CONSTRAINED,
	PROG_PROC_BRCTRL_MAX
} prog_proc_brctrl_t;

/**
 * Prpgram-processor module instance bit-rate control type look-up table.
 */
typedef struct prog_proc_brctrl_type_lu_ctx_s {
	uint8_t value;
	const char *name;
} prog_proc_brctrl_type_lu_ctx_t;

/**
 * Program processor settings context structure.
 */
typedef struct prog_proc_settings_ctx_s {
	/**
	 * Output URL. Set to NULL to request output stream to be closed.
	 */
	char *output_url;
	/**
	 * Bit-rate control type.
	 * @see prog_proc_brctrl_t
	 */
	prog_proc_brctrl_t brctrl_type;
	/** //FIXME
	 * Constant bit-rate in Kbps.
	 */
	uint32_t cbr; // Kbps== bits/msec
	/**
	 * Set to '1' to indicate program-processor to clear the input bit-rate
	 * peak register (program-processor will automatically reset flag to zero
	 * when operation is performed).
	 */
	int flag_clear_input_bitrate_peak;
	/**
	 * Guard distance, in milliseconds, to be applied between the time-stamps
	 * of the packetized elementary streams and its associated PCR (at the
	 * output stream of the program-processor).
	 * This value sets the maximum guard applicable to any of the
	 * elementary streams in a program (guard value may be tuned at the
	 * elementary stream level, but always below this limit).
	 */
	int64_t max_ts_pcr_guard_msec;
	/**
	 * Minimum delay value, in milliseconds, to be applied to the STC for
	 * outputting packets.
	 */
	uint64_t min_stc_delay_output_msec;
	/**
	 * Program Map Section currently used by this processor.
	 */
	psi_section_ctx_t *psi_section_ctx_pms;
	/**
	 * Set to '1' to indicate program-processor to purge all the disassociated
	 * ES-processors (program-processor will automatically reset flag to zero
	 * when operation is performed).
	 */
	int flag_purge_disassociated_processors;
} prog_proc_settings_ctx_t;

/**
 * Program processor task (heavy weight processing) context structure.
 */
typedef struct prog_proc_tsk_ctx_s {
	/**
	 * Processor identifier
	 */
	int proc_id;
	/**
	 * Externally defined LOG module context structure instance.
	 */
	log_ctx_t *log_ctx;
	/**
	 * Program processor settings.
	 */
	volatile struct prog_proc_settings_ctx_s prog_proc_settings_ctx;
	/**
	 * Elementary stream processors module context structure.
	 */
	procs_ctx_t *procs_ctx_es;

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

static int prog_proc_rest_put(prog_proc_tsk_ctx_t *prog_proc_tsk_ctx,
		const char *str);
static int prog_proc_rest_get(prog_proc_tsk_ctx_t *prog_proc_tsk_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse);

static int prog_proc_settings_ctx_init(
		volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx,
		log_ctx_t *log_ctx);
static void prog_proc_settings_ctx_deinit(
		volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx,
		log_ctx_t *log_ctx);

static void* api_thr(void *t);

/**
 * Open an Elementary Stream processor instance.
 * This is an auxiliary wrapper function to launch ES processors.
 * @param procs_ctx
 * @param proc_name
 * @param proc_id
 * @param log_ctx
 * @return Status code STAT_SUCCESS or STAT_ERROR.
 */
static int es_procs_post(procs_ctx_t *procs_ctx, const char *proc_name,
		int proc_id_arg, log_ctx_t *log_ctx);

/* **** Implementations **** */

static const prog_proc_brctrl_type_lu_ctx_t prog_proc_brctrl_type_lutable
		[PROG_PROC_BRCTRL_MAX]=
{
		{PROG_PROC_BRCTRL_NONE, "None"},
		{PROG_PROC_BRCTRL_CBR, "CBR"},
		{PROG_PROC_BRCTRL_VBR_CONSTRAINED,
				"Constrained VBR (CVBR)- Max. bitrate"}
};

/**
 * Supported codec types that will be registered by this module.
 */
#define SUPP_CODECS_NUM 1
static const proc_if_t *proc_if_supp_codecs[SUPP_CODECS_NUM]= {
		&proc_if_bypass
};

/**
 * Program processor task.
 * Arguments are:<br>
 *     - This application name (typical argv[0])
 *     - Character string specifying 'href' for this program processors
 *     - Settings character string (see settings description below).
 * .
 * <br>
 * Settings character string has the following JSON-formatted structure:
 * {
 *     "output_url":string,
 *     "brctrl_type_selector":
 *     [
 *         {"name":string, "value":number},
 *         ...
 *     ]
 *     "selected_brctrl_type_value":number,
 *     "cbr":number,
 *     "flag_clear_input_bitrate_peak":boolean,
 *     "flag_purge_disassociated_processors":boolean,
 *     "pmt_octet_stream":string, -base64 encoded Program Map Table binary-
 *     "max_ts_pcr_guard_msec":number,
 *     "min_stc_delay_output_msec":number
 * }
 */
int main(int argc, char *argv[], char *envp[])
{
	int ret_code, end_code= STAT_ERROR;
	const char *href_arg= (const char*)argv[1];
	const char *settings_str= (const char*)argv[2];
	size_t href_arg_len= 0;
	prog_proc_tsk_ctx_t *prog_proc_tsk_ctx= NULL;
	fifo_ctx_t *iput_fifo_ctx= NULL, *oput_fifo_ctx= NULL;
    uint8_t *pkt_iput= NULL;
    size_t fifo_elem_size= 0;
    pthread_t api_thread;
    void *thread_end_code= NULL;
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
	LOGD("===============================================argvc: %d; argv[0]: '%s'; href_arg: '%s'; settings_str: '%s'\n",
			argc, argv[0], href_arg, settings_str); //FIXME!!

	/* Open PROCS module */
	ret_code= procs_module_open(NULL);
	CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_NOTMODIFIED, goto end);

	/* Get context structure (creates and initializes) */
	prog_proc_tsk_ctx= prog_proc_tsk_open(settings_str, href_arg,
			LOG_CTX_GET());
	CHECK_DO(prog_proc_tsk_ctx!= NULL, goto end);

	/* Get input/output FIFO buffers */
	iput_fifo_ctx= prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_IPUT];
	oput_fifo_ctx= prog_proc_tsk_ctx->fifo_ctx_io_array[PROC_OPUT];
	CHECK_DO(iput_fifo_ctx!= NULL && oput_fifo_ctx!= NULL, goto end);

	/* Launch API input/output thread */
	ret_code= pthread_create(&api_thread, NULL, api_thr,
			(void*)prog_proc_tsk_ctx);
	CHECK_DO(ret_code== 0, goto end);

	/* Program-processing function of our heavy weight task */
	// FIXME!!: explain prog_procs (ESs) unblocking in API thread!!!!!!!!!!!!!!!!!!
	while(*prog_proc_tsk_ctx->ref_flag_exit_shared== 0) {
		int ret_code;
		uint16_t pid;
		proc_frame_ctx_t proc_frame_ctx= {};

		LOGD(">>>>>>>>>>>>>>>>>>testing-loop!!!\n"); //FIXME!!

		/* Get input packet from FIFO buffer */
		if(pkt_iput!= NULL) {
			free(pkt_iput);
			pkt_iput= NULL;
			fifo_elem_size= 0;
		}
		if(fifo_get(iput_fifo_ctx, (void**)&pkt_iput, &fifo_elem_size)!=
				STAT_SUCCESS) {
			schedule(); // schedule to avoid closed loops
			continue; // Discard packet
		}

		/* Parse MPEG2-TS PPID */
		pid= TS_BUF_GET_PID(pkt_iput);

		/* Send packet to corresponding elementary stream (ES) processor */
	    proc_frame_ctx.data= pkt_iput;
	    proc_frame_ctx.p_data[0]= pkt_iput;
	    proc_frame_ctx.linesize[0]= sizeof(fifo_elem_size);
	    proc_frame_ctx.width[0]= sizeof(fifo_elem_size);
	    proc_frame_ctx.height[0]= 1;
	    proc_frame_ctx.proc_sample_fmt= PROC_IF_FMT_UNDEF;
	    proc_frame_ctx.pts= -1;
	    proc_frame_ctx.dts= -1;
	    proc_frame_ctx.es_id= pid;
	    ret_code= procs_send_frame(prog_proc_tsk_ctx->procs_ctx_es, pid,
	    		&proc_frame_ctx);
	    ASSERT(ret_code== STAT_SUCCESS || ret_code== STAT_ENOTFOUND);
	}

	end_code= STAT_SUCCESS;
end:
	/* **** Release task related resources **** */
	if(pkt_iput!= NULL) {
		free(pkt_iput);
		pkt_iput= NULL;
		fifo_elem_size= 0;
	}

	LOGD("Waiting processor API-thread to join... ");
	pthread_join(api_thread, &thread_end_code);
	if(thread_end_code!= NULL) {
		ASSERT(*((int*)thread_end_code)== STAT_SUCCESS);
		free(thread_end_code);
		thread_end_code= NULL;
	}
	LOGD("joined O.K.\n");

	/* Release context structure */
	prog_proc_tsk_close(&prog_proc_tsk_ctx);

	/* Release PROCS module */
	procs_module_close();

	/* Remember we opened our own LOG module instance... release it */
	log_close(&LOG_CTX_GET());

	/* Release LOG module */
	log_module_close();

	return (end_code== STAT_SUCCESS)? EXIT_SUCCESS: EXIT_FAILURE;
}

static prog_proc_tsk_ctx_t* prog_proc_tsk_open(const char *settings_str,
		const char* href_arg, log_ctx_t *log_ctx)
{
	char *p, *end_p;
	int i, proc_id, ret_code, end_code= STAT_ERROR, shm_fd= -1;
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
	psi_pms_ctx_t *psi_pms_ctx= NULL; // Do not release
	llist_t *n;
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

	/* Get processor identifier from href */
	p= strrchr(href_arg, '/');
	CHECK_DO(p!= NULL, goto end);
	proc_id= strtol(++p, &end_p, 10);
	CHECK_DO(end_p> p, goto end);
	prog_proc_tsk_ctx->proc_id= proc_id;
	LOGD("Parsed processor Id. is %d\n", proc_id);

	/* Set LOGs context */
	prog_proc_tsk_ctx->log_ctx= log_ctx;

	/* Get settings structure */
	prog_proc_settings_ctx= &prog_proc_tsk_ctx->prog_proc_settings_ctx;

	/* Initialize settings to defaults */
	ret_code= prog_proc_settings_ctx_init(prog_proc_settings_ctx,
			LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Parse and put given settings */
	ret_code= prog_proc_rest_put(prog_proc_tsk_ctx, settings_str);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Elementary-stream processors module context structure */
	prog_proc_tsk_ctx->procs_ctx_es= procs_open(LOG_CTX_GET(),
			TS_MAX_PID_VAL+ 1, "es_processors", "FIXME!!"); //FIXME: href in 'procs_open()'
	CHECK_DO(prog_proc_tsk_ctx->procs_ctx_es!= NULL, goto end);

	/* Register necessary processors types.
	 * Note that return value 'STAT_ECONFLICT' means that processor was
	 * already registered, so for this case is also O.K.
	 */
	for(i= 0; i< SUPP_CODECS_NUM; i++) {
		ret_code= procs_module_opt("PROCS_REGISTER_TYPE",
				proc_if_supp_codecs[i]);
		CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_ECONFLICT,
				goto end);
	}

	/* Register Elementary Stream (ES) processors */
	psi_pms_ctx= prog_proc_settings_ctx->psi_section_ctx_pms->data;
	CHECK_DO(psi_pms_ctx!= NULL, goto end);
	for(n= psi_pms_ctx->psi_pms_es_ctx_llist; n!= NULL; n= n->next) {
		psi_pms_es_ctx_t *psi_pms_es_ctx;
		uint16_t es_pid;

		psi_pms_es_ctx= (psi_pms_es_ctx_t*)n->data;
		CHECK_DO(psi_pms_es_ctx!= NULL, continue);
		es_pid= psi_pms_es_ctx->elementary_PID;

//		/* Get PID-processor type */ //FIXME!!: TODO
//		if(type_id!= NULL) {
//			free(type_id);
//			type_id= NULL;
//		}
//		ret_code= pid_proc_module_opt(PID_PROC_MODULE_GUESS_PROC_U_TYPE,
//				(const psi_pms_es_ctx_t*)psi_pms_es_ctx, &type_id);
//		CHECK_DO(ret_code== SUCCESS && type_id!= NULL, goto end);

		/* Register Elementary Stream processor */
		CHECK_DO(es_procs_post(prog_proc_tsk_ctx->procs_ctx_es, "bypass",
				es_pid, LOG_CTX_GET())== STAT_SUCCESS, goto end);
	}

	/* **** Open shared memory pointers/references to communicate **** */

	/* Compose FIFO name prefix using 'href' */
	CHECK_DO(href_arg_len< PROCS_HREF_MAX_LEN, goto end);
	memcpy(href, href_arg, href_arg_len);
	p= href;
	while((p= strchr(p, '/'))!= NULL && (p< href+ PROCS_HREF_MAX_LEN)) {
		*p= '-';
	}
	LOGD("href_arg-modified: '%s'\n", href);

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

	/* Release elementary-stream processors module context structure */
	procs_close(&prog_proc_tsk_ctx->procs_ctx_es);

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
 * Implements the proc_if_s::rest_put callback.
 * See .proc_if.h for further details.
 */
static int prog_proc_rest_put(prog_proc_tsk_ctx_t *prog_proc_tsk_ctx,
		const char *str)
{
	int flag_repres_type, ret_code, end_code= STAT_ERROR;
	volatile prog_proc_settings_ctx_t *prog_proc_settings_ctx= NULL;
	cJSON *cjson_rest= NULL, *cjson_aux= NULL;
	char *selected_brctrl_type_value_str= NULL, *cbr_str= NULL,
			*flag_clear_input_bitrate_peak_str= NULL,
			*flag_purge_disassociated_processors_str= NULL, *pms_str= NULL,
			*ts_pcr_guard_str= NULL, *stc_delay_output_str= NULL;
	uint8_t *buf_pmt= NULL;
	psi_section_ctx_t *psi_section_ctx_pmt= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(prog_proc_tsk_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(str!= NULL, return STAT_ERROR);

	LOG_CTX_SET(prog_proc_tsk_ctx->log_ctx);

	/* Get settings context */
	prog_proc_settings_ctx= &prog_proc_tsk_ctx->prog_proc_settings_ctx;

	/* Guess string representation format (JSON-REST or Query) */
	flag_repres_type=
			(strlen(str)> 0 && str[0]=='{' && str[strlen(str)-1]=='}')?
					STR_JSON_REST: STR_URL_QUERY;

	/* In the case string format is JSON-REST, parse to structure */
	if(flag_repres_type== STR_JSON_REST) {
		cjson_rest= cJSON_Parse(str);
		CHECK_DO(cjson_rest!= NULL, goto end);
	}

	/* **** PUT specific processor settings ****
	 * (Parse query-string to get settings parameters)
	 */

	/* Output URL */
	if(flag_repres_type== STR_URL_QUERY) {
		char *output_url= uri_parser_query_str_get_value("output_url", str);
		if(output_url!= NULL) {
			if(prog_proc_settings_ctx->output_url!= NULL)
				free(prog_proc_settings_ctx->output_url);
			prog_proc_settings_ctx->output_url= output_url;
		}
	} else if(flag_repres_type== STR_JSON_REST) {
		cjson_aux= cJSON_GetObjectItem(cjson_rest, "output_url");
		if(cjson_aux!= NULL) {
			char *valuestring= cjson_aux->valuestring;
			if(prog_proc_settings_ctx->output_url!= NULL)
				free(prog_proc_settings_ctx->output_url);
			prog_proc_settings_ctx->output_url= (valuestring!= NULL)?
					strdup(valuestring): NULL;
		}
	}

	/* Bit-rate control type */
	if(flag_repres_type== STR_URL_QUERY) {
		selected_brctrl_type_value_str= uri_parser_query_str_get_value(
				"selected_brctrl_type_value", str);
		if(selected_brctrl_type_value_str!= NULL)
			prog_proc_settings_ctx->brctrl_type= atoi(
					selected_brctrl_type_value_str);
	} else if(flag_repres_type== STR_JSON_REST) {
		cjson_aux= cJSON_GetObjectItem(cjson_rest,
				"selected_brctrl_type_value");
		if(cjson_aux!= NULL)
			prog_proc_settings_ctx->brctrl_type= cjson_aux->valueint;
	}

	/* CBR value */
	if(flag_repres_type== STR_URL_QUERY) {
		cbr_str= uri_parser_query_str_get_value("cbr", str);
		if(cbr_str!= NULL)
			prog_proc_settings_ctx->cbr= (uint32_t)atoll(cbr_str);
	} else if(flag_repres_type== STR_JSON_REST) {
		cjson_aux= cJSON_GetObjectItem(cjson_rest, "cbr");
		if(cjson_aux!= NULL)
			prog_proc_settings_ctx->cbr= (uint32_t)cjson_aux->valueint;
	}

	/* Flag to clear input bit-rate peak register */
	if(flag_repres_type== STR_URL_QUERY) {
		flag_clear_input_bitrate_peak_str= uri_parser_query_str_get_value(
				"flag_clear_input_bitrate_peak", str);
		if(flag_clear_input_bitrate_peak_str!= NULL)
			prog_proc_settings_ctx->flag_clear_input_bitrate_peak=
					(strncmp(flag_clear_input_bitrate_peak_str, "true",
							strlen("true"))== 0)? 1: 0;
	} else if(flag_repres_type== STR_JSON_REST) {
		cjson_aux= cJSON_GetObjectItem(cjson_rest,
				"flag_clear_input_bitrate_peak");
		if(cjson_aux!= NULL)
			prog_proc_settings_ctx->flag_clear_input_bitrate_peak=
					(cjson_aux->type==cJSON_True)?1 : 0;
	}

	/* TS-PCR guard distance value */
	if(flag_repres_type== STR_URL_QUERY) {
		ts_pcr_guard_str= uri_parser_query_str_get_value(
				"max_ts_pcr_guard_msec", str);
		if(ts_pcr_guard_str!= NULL)
			prog_proc_settings_ctx->max_ts_pcr_guard_msec= (int64_t)
			atoll(ts_pcr_guard_str);
	} else if(flag_repres_type== STR_JSON_REST) {
		cjson_aux= cJSON_GetObjectItem(cjson_rest, "max_ts_pcr_guard_msec");
		if(cjson_aux!= NULL)
			prog_proc_settings_ctx->max_ts_pcr_guard_msec= (int64_t)
			cjson_aux->valuedouble;
	}

	/* Program-processor minimum delay */
	if(flag_repres_type== STR_URL_QUERY) {
		stc_delay_output_str= uri_parser_query_str_get_value(
				"min_stc_delay_output_msec", str);
		if(stc_delay_output_str!= NULL)
			prog_proc_settings_ctx->min_stc_delay_output_msec= (uint64_t)
			atoll(stc_delay_output_str);
	} else if(flag_repres_type== STR_JSON_REST) {
		cjson_aux= cJSON_GetObjectItem(cjson_rest, "min_stc_delay_output_msec");
		if(cjson_aux!= NULL)
			prog_proc_settings_ctx->min_stc_delay_output_msec= (uint64_t)
			cjson_aux->valuedouble;
	}

	/* PMS */
	if(flag_repres_type== STR_URL_QUERY) {
		pms_str= uri_parser_query_str_get_value("pmt_octet_stream", str);
	} else if(flag_repres_type== STR_JSON_REST) {
		cjson_aux= cJSON_GetObjectItem(cjson_rest, "pmt_octet_stream");
		if(cjson_aux!= NULL)
			pms_str= cJSON_PrintUnformatted(cjson_aux);
	}
	if(pms_str!= NULL) {
		size_t pms_str_size, olen= 0;

		/* Decode PMT into binary buffer:
		 * - Get size of PMT decoded data using 'mbedtls_base64_decode',
		 * - Allocate decoded data buffer,
		 * - Get decoded PMT data into buffer.
		 */
		pms_str_size= strlen(pms_str)
				- 2; // '-2' to exclude character '"' limiting the JSON string;
		CHECK_DO(pms_str_size> 0, goto end);

		ret_code= mbedtls_base64_decode(NULL, 0, &olen,
				(const unsigned char*)pms_str+ 1, // '+1' to skip '"'
				pms_str_size);
		CHECK_DO(ret_code== MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && olen> 0,
				goto end);

		buf_pmt= (uint8_t*)malloc(olen);
		CHECK_DO(buf_pmt!= NULL, goto end);
		ret_code= mbedtls_base64_decode(buf_pmt, olen, &olen,
				(const unsigned char*)pms_str+ 1, pms_str_size);
		CHECK_DO(ret_code== 0, goto end);

		/* Parse binary PMT into specific context structure */
		ret_code= psi_dec_section(buf_pmt, olen, prog_proc_tsk_ctx->proc_id,
				LOG_CTX_GET(), &psi_section_ctx_pmt);
		CHECK_DO(ret_code== STAT_SUCCESS && psi_section_ctx_pmt!= NULL,
				goto end);

		psi_section_ctx_release(&prog_proc_settings_ctx->psi_section_ctx_pms);
		prog_proc_settings_ctx->psi_section_ctx_pms= psi_section_ctx_pmt;
		psi_section_ctx_pmt= NULL; // Avoid double referencing
		psi_section_ctx_trace(
				prog_proc_settings_ctx->psi_section_ctx_pms); //comment-me
	}

	/* Flag to purge disassociated ES-processors */
	if(flag_repres_type== STR_URL_QUERY) {
		flag_purge_disassociated_processors_str=
				uri_parser_query_str_get_value(
						"flag_purge_disassociated_processors", str);
		if(flag_purge_disassociated_processors_str!= NULL)
			prog_proc_settings_ctx->flag_purge_disassociated_processors=
					(strncmp(flag_purge_disassociated_processors_str, "true",
							strlen("true"))== 0)? 1: 0;
	} else if(flag_repres_type== STR_JSON_REST) {
		cjson_aux= cJSON_GetObjectItem(cjson_rest,
				"flag_purge_disassociated_processors");
		if(cjson_aux!= NULL)
			prog_proc_settings_ctx->flag_purge_disassociated_processors=
					(cjson_aux->type==cJSON_True)?1 : 0;
	}

	end_code= STAT_SUCCESS;
end:
	if(cjson_rest!= NULL)
		cJSON_Delete(cjson_rest);
	if(selected_brctrl_type_value_str!= NULL)
		free(selected_brctrl_type_value_str);
	if(cbr_str!= NULL)
		free(cbr_str);
	if(flag_clear_input_bitrate_peak_str!= NULL)
		free(flag_clear_input_bitrate_peak_str);
	if(flag_purge_disassociated_processors_str!= NULL)
		free(flag_purge_disassociated_processors_str);
	if(pms_str!= NULL)
		free(pms_str);
	if(ts_pcr_guard_str!= NULL)
		free(ts_pcr_guard_str);
	if(stc_delay_output_str!= NULL)
		free(stc_delay_output_str);
	if(buf_pmt!= NULL) {
		free(buf_pmt);
		buf_pmt= NULL;
	}
	if(psi_section_ctx_pmt!= NULL)
		psi_section_ctx_release(&psi_section_ctx_pmt);
	return end_code;
}

//FIXME!!:TODO !!
static int prog_proc_rest_get(prog_proc_tsk_ctx_t *prog_proc_tsk_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse)
{

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

	/* **** Initialize specific processor settings **** */

	prog_proc_settings_ctx->output_url= NULL;

	prog_proc_settings_ctx->brctrl_type= PROG_PROC_BRCTRL_NONE;

	prog_proc_settings_ctx->cbr= 1024; //[Kbps] //FIXME: is this init. value good?

	prog_proc_settings_ctx->flag_clear_input_bitrate_peak= 0;

	// 'max_ts_pcr_guard_msec' is set later by user

	// 'min_stc_delay_output_msec' is set later by user

	prog_proc_settings_ctx->psi_section_ctx_pms= NULL;

	prog_proc_settings_ctx->flag_purge_disassociated_processors= 0;

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

	if(prog_proc_settings_ctx->output_url!= NULL) {
		free(prog_proc_settings_ctx->output_url);
		prog_proc_settings_ctx->output_url= NULL;
	}

	psi_section_ctx_release(&prog_proc_settings_ctx->psi_section_ctx_pms);

	// Reserved for future use
}

static void* api_thr(void *t)
{
	fifo_ctx_t *iput_api_fifo_ctx, *oput_api_fifo_ctx;
	prog_proc_tsk_ctx_t *prog_proc_tsk_ctx= (prog_proc_tsk_ctx_t*)t;
	int i, ret_code, *ref_end_code= NULL;
	char *api_msg= NULL, *resp= NULL, wrap_resp= NULL; // release-me (loop)
	cJSON *cjson_msg= NULL; // release-me (loop)
	LOG_CTX_INIT(NULL);

	/* Allocate return context; initialize to a default 'ERROR' value */
	ref_end_code= (int*)malloc(sizeof(int));
	CHECK_DO(ref_end_code!= NULL, return NULL);
	*ref_end_code= STAT_ERROR;

	/* Check arguments */
	CHECK_DO(prog_proc_tsk_ctx!= NULL, goto end);

	LOG_CTX_SET(prog_proc_tsk_ctx->log_ctx);

	/* Get API input/output FIFO buffers */
	iput_api_fifo_ctx= prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_IPUT];
	oput_api_fifo_ctx= prog_proc_tsk_ctx->fifo_ctx_api_array[PROC_OPUT];
	CHECK_DO(iput_api_fifo_ctx!= NULL && oput_api_fifo_ctx!= NULL, goto end);

	/* Run processing thread */
	while(*prog_proc_tsk_ctx->ref_flag_exit_shared== 0) {
		char *method, *msg;
		size_t api_msg_len;
		cJSON *cjson1, *cjson2; // auxiliary variables

		if(api_msg!= NULL) {
			free(api_msg);
			api_msg= NULL;
			api_msg_len= 0;
		}
		if(cjson_msg!= NULL) {
			cJSON_Delete(cjson_msg);
			cjson_msg= NULL;
		}
		if(resp!= NULL) {
			free(resp);
			resp= NULL;
		}
		if(wrap_resp!= NULL) {
			free(wrap_resp);
			wrap_resp= NULL;
		}

#if 1
		schedule();
#else
		/* Get GET/PUT messages from input API FIFO */
		ret_code= fifo_get(iput_api_fifo_ctx, (void**)&api_msg, &api_msg_len);
		if(ret_code== STAT_EAGAIN)
			break; // FIFO was unblocked to finalize
		CHECK_DO(ret_code==STAT_SUCCESS, schedule(); continue);

		/* Parse "method" and "message" fields from API message; format is:
		 * {
		 *     "method": string, // may be "GET" or "PUT"
		 *     "message": string
		 * }
		 */
		CHECK_DO(api_msg== NULL || api_msg_len== 0 ||
				(cjson_msg= cJSON_Parse(api_msg))== NULL ||
				(cjson1= cJSON_GetObjectItem(cjson_msg, "method"))== NULL ||
				(method= cjson1->valuestring)== NULL ||
				(cjson2= cJSON_GetObjectItem(cjson_msg, "message"))== NULL ||
				(msg= cjson2->valuestring)!= NULL, continue);

		/* Process API message */
		if(strcmp(method, "GET")== 0) {
			ret_code= prog_proc_rest_get(prog_proc_tsk_ctx,
					PROC_IF_REST_FMT_CHAR, &resp);
		} else if(strcmp(method, "PUT")== 0) {
			ret_code= prog_proc_rest_put(prog_proc_tsk_ctx, msg);
		}

		/* Wrap end code and response string if applicable. (note that
		 * response string may be NULL, in that case we will only wrap
		 * the return code).
		 */
		wrap_resp= wrap_response(ret_code, resp, method, LOG_CTX_GET());
		CHECK_DO(wrap_resp!= NULL, continue);

		/* Return response to calling process application */
		ret_code= fifo_put(oput_api_fifo_ctx, (void**)&wrap_resp,
				strlen(wrap_resp));
		if(ret_code== STAT_ENOMEM)
			break; // FIFO was unblocked to finalize
		CHECK_DO(ret_code==STAT_SUCCESS, schedule(); continue);
#endif
	}

	*ref_end_code= STAT_SUCCESS;
end:
	if(api_msg!= NULL)
		free(api_msg);
	if(cjson_msg!= NULL)
		cJSON_Delete(cjson_msg);
	if(resp!= NULL)
		free(resp);
	if(wrap_resp!= NULL)
		free(wrap_resp);
	// FIXME!!: explain!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	for(i= 0; i< (TS_MAX_PID_VAL+ 1); i++) {
		if(prog_proc_tsk_ctx->procs_ctx_es!= NULL) {
			register int ret_code= procs_opt(prog_proc_tsk_ctx->procs_ctx_es,
					"PROCS_ID_DELETE", i);
			ASSERT(ret_code== STAT_SUCCESS || ret_code== STAT_ENOTFOUND);
		}
	}
	return (void*)ref_end_code;
}

static int es_procs_post(procs_ctx_t *procs_ctx, const char *proc_name,
		int proc_id_arg, log_ctx_t *log_ctx)
{
	int proc_id, ret_code, end_code= STAT_ERROR;
	char *rest_str= NULL;
	cJSON *cjson_rest= NULL, *cjson_aux= NULL;
	const char *settings_fmt= "forced_proc_id=%d";
	char settings[strlen(settings_fmt)+ 32+ 1];
	LOG_CTX_INIT(log_ctx);

	LOGD("Opening ES-processor type: '%s'; PID: %u\n", proc_name, proc_id_arg);

	snprintf(settings, sizeof(settings), settings_fmt, proc_id_arg);

	ret_code= procs_opt(procs_ctx, "PROCS_POST", proc_name, settings,
			&rest_str);
	CHECK_DO(ret_code== STAT_SUCCESS && rest_str!= NULL, goto end);

	cjson_rest= cJSON_Parse(rest_str);
	CHECK_DO(cjson_rest!= NULL, goto end);

	cjson_aux= cJSON_GetObjectItem(cjson_rest, "proc_id");
	CHECK_DO(cjson_aux!= NULL, goto end);

	proc_id= cjson_aux->valuedouble;
	CHECK_DO(proc_id== proc_id_arg, goto end);

	 end_code= STAT_SUCCESS;
end:
	if(rest_str!= NULL)
		free(rest_str);
	if(cjson_rest!= NULL)
		cJSON_Delete(cjson_rest);
	return end_code;
}
