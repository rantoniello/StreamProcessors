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
 * @file psi_proc.c
 * @author Rafael Antoniello
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "psi_proc.h"

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/schedule.h>
#include <libmediaprocsutils/llist.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocs/proc.h>
#include "ts.h"
//#include "ts_dec.h"
#include "psi.h"
#include "psi_dec.h"
#include "psi_table.h"
#include "psi_table_dec.h"

/* **** Definitions **** */

/**
 * Returns non-zero if 'tag' string is equal to given TAG string.
 */
#define TAG_IS(TAG) (strcmp(tag, TAG)== 0)

/**
 * PSI common processor context structure.
 */
typedef struct psi_proc_ctx_s {
	/**
	 * Generic processor context structure.
	 * *MUST* be the first field in order to be able to cast to proc_ctx_t.
	 */
	struct proc_ctx_s proc_ctx;
	/**
	 * Processor's table, section, ... mutual exclusion lock
	 */
	pthread_mutex_t psi_opaque_ctx_mutex;
	/**
	 * MPEG2-TS Continuity Counter (CC) for input PSI stream.
	 */
	uint8_t tscc_input;
} psi_proc_ctx_t;

/**
 * PSI table processor context structure.
 */
typedef struct psi_table_proc_ctx_s {
	/**
	 * PSI common processor context structure.
	 * *MUST* be the first field in order to be able to cast to both
	 * proc_ctx_t and psi_proc_ctx_t.
	 */
	struct psi_proc_ctx_s psi_proc_ctx;
	/**
	 * Current table context structure (last actualized version).
	 * This table will be accessed concurrently (use mutual exclusion).
	 */
	psi_table_ctx_t *psi_table_ctx;
} psi_table_proc_ctx_t;

/**
 * PSI section processor context structure.
 */
typedef struct psi_section_proc_ctx_s {
	/**
	 * PSI common processor context structure.
	 * *MUST* be the first field in order to be able to cast to both
	 * proc_ctx_t and psi_proc_ctx_t.
	 */
	struct psi_proc_ctx_s psi_proc_ctx;
	/**
	 * Current section context structure (last actualized version).
	 * This section will be accessed concurrently (use mutual exclusion).
	 */
	psi_section_ctx_t *psi_section_ctx;
} psi_section_proc_ctx_t;

/* **** Prototypes **** */

/* **** PSI common functions **** */

static int psi_proc_ctx_init(psi_proc_ctx_t *psi_proc_ctx,
		const proc_if_t *proc_if, const char *settings_str, log_ctx_t *log_ctx,
		va_list arg);
static void psi_proc_ctx_deinit(psi_proc_ctx_t *psi_proc_ctx);
static int proc_send_frame_with_tspkt(proc_ctx_t *proc_ctx,
		const proc_frame_ctx_t *proc_frame_ctx);

/* **** PSI table processor **** */

static proc_ctx_t* psi_table_proc_open(const proc_if_t *proc_if,
		const char *settings_str, const char* href, log_ctx_t *log_ctx,
		va_list arg);
static void psi_table_proc_close(proc_ctx_t **ref_proc_ctx);
static int psi_table_proc_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t *iput_fifo_ctx, fifo_ctx_t *oput_fifo_ctx);
static int psi_table_proc_opt(proc_ctx_t *proc_ctx, const char *tag,
		va_list arg);
static int psi_table_proc_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse);

/* **** PSI section processor **** */

static proc_ctx_t* psi_section_proc_open(const proc_if_t *proc_if,
		const char *settings_str, const char* href, log_ctx_t *log_ctx,
		va_list arg);
static void psi_section_proc_close(proc_ctx_t **ref_proc_ctx);
static int psi_section_proc_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t *iput_fifo_ctx, fifo_ctx_t *oput_fifo_ctx);
static int psi_section_proc_opt(proc_ctx_t *proc_ctx, const char *tag,
		va_list arg);
static int psi_section_proc_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse);

/* **** Implementations **** */

const proc_if_t proc_if_psi_table_proc=
{
	"psi_table_proc", "parser", "n/a",
	(uint64_t)0,
	psi_table_proc_open,
	psi_table_proc_close,
	proc_send_frame_with_tspkt,
	NULL, // send-no-dup
	NULL, // proc_recv_frame
	NULL, // no specific unblock function extension
	NULL, // 'proc_rest_put()'
	NULL, // Implemented indirectly in 'psi_table_proc_opt()'
	psi_table_proc_process_frame,
	psi_table_proc_opt, // indirectly calls 'psi_table_proc_rest_get()'
	NULL, // 'iput_fifo_elem_opaque_dup()'
	NULL, // 'iput_fifo_elem_opaque_release()'
	NULL, // 'oput_fifo_elem_opaque_dup()'
};

const proc_if_t proc_if_psi_section_proc=
{
	"psi_section_proc", "parser", "n/a",
	(uint64_t)0,
	psi_section_proc_open,
	psi_section_proc_close,
	proc_send_frame_with_tspkt,
	NULL, // send-no-dup
	NULL, // proc_recv_frame
	NULL, // no specific unblock function extension
	NULL, // 'proc_rest_put()'
	NULL, // Implemented indirectly in 'psi_section_proc_opt()'
	psi_section_proc_process_frame,
	psi_section_proc_opt, // indirectly calls 'psi_section_proc_rest_get()'
	NULL, // 'iput_fifo_elem_opaque_dup()'
	NULL, // 'iput_fifo_elem_opaque_release()'
	NULL, // 'oput_fifo_elem_opaque_dup()'
};

/* **** PSI common functions **** */

static int psi_proc_ctx_init(psi_proc_ctx_t *psi_proc_ctx,
		const proc_if_t *proc_if, const char *settings_str, log_ctx_t *log_ctx,
		va_list arg)
{
	int ret_code, end_code= STAT_ERROR;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(psi_proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(proc_if!= NULL, return STAT_ERROR);
	CHECK_DO(settings_str!= NULL, return STAT_ERROR);
	// Note: 'log_ctx' is allowed to be NULL

	ret_code= pthread_mutex_init(&psi_proc_ctx->psi_opaque_ctx_mutex, NULL);
	CHECK_DO(ret_code== 0, goto end);

	psi_proc_ctx->tscc_input= TS_CC_UNDEF;

	// Reserved for future use: initialize other new variables here...

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		psi_proc_ctx_deinit(psi_proc_ctx);
	return end_code;
}

static void psi_proc_ctx_deinit(psi_proc_ctx_t *psi_proc_ctx)
{
	LOG_CTX_INIT(NULL);

	if(psi_proc_ctx== NULL)
		return;

	LOG_CTX_SET(((proc_ctx_t*)psi_proc_ctx)->log_ctx);

	/* Release mutex */
	ASSERT(pthread_mutex_destroy(&psi_proc_ctx->psi_opaque_ctx_mutex)== 0);

	// Reserved for future use: release other new variables here...
}

static int proc_send_frame_with_tspkt(proc_ctx_t *proc_ctx,
		const proc_frame_ctx_t *proc_frame_ctx)
{
	int ret_code, end_code= STAT_ERROR;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(proc_frame_ctx!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	/* Perform some sanity checks */
	CHECK_DO(proc_frame_ctx->data!= NULL, goto end);
	CHECK_DO(proc_frame_ctx->data[0]== 0x47, goto end);
	CHECK_DO(proc_frame_ctx->width[0]== TS_PKT_SIZE, goto end);

	/* Check PID value (actually should never change) */
	CHECK_DO(proc_frame_ctx->es_id== proc_ctx->proc_instance_index, goto end);

	/* Write frame to input FIFO */
	ret_code= fifo_put_dup(proc_ctx->fifo_ctx_array[PROC_IPUT],
			proc_frame_ctx->data, TS_PKT_SIZE);
	CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_ENOMEM, goto end);

	end_code= STAT_SUCCESS;
end:
	return end_code;
}

/* **** PSI table processor **** */

/**
 * Implements the proc_if_s::open callback.
 * See .proc_if.h for further details.
 */
static proc_ctx_t* psi_table_proc_open(const proc_if_t *proc_if,
		const char *settings_str, const char* href, log_ctx_t *log_ctx,
		va_list arg)
{
	int ret_code, end_code= STAT_ERROR;
	psi_table_proc_ctx_t *psi_table_proc_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(proc_if!= NULL, return NULL);
	CHECK_DO(settings_str!= NULL, return NULL);
	// Parameter 'href' is allowed to be NULL
	// Parameter 'log_ctx' is allowed to be NULL

	/* Allocate context structure */
	psi_table_proc_ctx= (psi_table_proc_ctx_t*)calloc(1, sizeof(
			psi_table_proc_ctx_t));
	CHECK_DO(psi_table_proc_ctx!= NULL, goto end);

	/* **** Initialize context structure **** */

	/* Initialize PSI processors common structure */
	ret_code= psi_proc_ctx_init((psi_proc_ctx_t*)psi_table_proc_ctx, proc_if,
			settings_str, LOG_CTX_GET(), arg);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	psi_table_proc_ctx->psi_table_ctx= NULL;

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		psi_table_proc_close((proc_ctx_t**)&psi_table_proc_ctx);
	return (proc_ctx_t*)psi_table_proc_ctx;
}

/**
 * Implements the proc_if_s::close callback.
 * See .proc_if.h for further details.
 */
static void psi_table_proc_close(proc_ctx_t **ref_proc_ctx)
{
	psi_table_proc_ctx_t *psi_table_proc_ctx;
	//LOG_CTX_INIT(NULL);

	if(ref_proc_ctx== NULL ||
			(psi_table_proc_ctx= (psi_table_proc_ctx_t*)*ref_proc_ctx)== NULL)
		return;

	//LOG_CTX_SET(((proc_ctx_t*)psi_table_proc_ctx)->log_ctx);

	/* De-initialize PSI processors common structure */
	psi_proc_ctx_deinit((psi_proc_ctx_t*)psi_table_proc_ctx);

	/* Release parsed table */
	psi_table_ctx_release(&psi_table_proc_ctx->psi_table_ctx);

	/* Release context structure */
	free(psi_table_proc_ctx);
	*ref_proc_ctx= NULL;
}

/**
 * Implements the proc_if_s::process_frame callback.
 * See .proc_if.h for further details.
 */
static int psi_table_proc_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t* iput_fifo_ctx, fifo_ctx_t* oput_fifo_ctx)
{
	int new_table_version_found, ret_code, end_code= STAT_ERROR;
	psi_table_proc_ctx_t *psi_table_proc_ctx= NULL; // Do not release (alias)
	psi_proc_ctx_t *psi_proc_ctx= NULL; // Do not release (alias)
	pthread_mutex_t *psi_table_ctx_mutex_p= NULL; // Do not release
	psi_table_ctx_t *psi_table_ctx= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(iput_fifo_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(oput_fifo_ctx!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	/* Get PSI table processor context structure and related fields */
	psi_table_proc_ctx= (psi_table_proc_ctx_t*)proc_ctx;
	psi_proc_ctx= (psi_proc_ctx_t*)proc_ctx;
	psi_table_ctx_mutex_p= &psi_proc_ctx->psi_opaque_ctx_mutex;

	/* Get next table from input stream */
	ret_code= psi_table_dec_get_next_table(iput_fifo_ctx, LOG_CTX_GET(),
			&psi_proc_ctx->tscc_input, proc_ctx->proc_instance_index/*PID*/,
			&psi_table_ctx);
	if(ret_code!= STAT_SUCCESS) {
		end_code= ret_code;
		goto end;
	}
	CHECK_DO(psi_table_ctx!= NULL, goto end);

	/* **** Update stored table copy **** */

	/* Check if table has changed */
	new_table_version_found= psi_table_proc_ctx->psi_table_ctx== NULL ||
			psi_table_ctx_cmp(psi_table_ctx,
					psi_table_proc_ctx->psi_table_ctx)!= 0;

	/* If new table version found, store into context
	 * (manage context members in mutual exclusion)
	 */
	if(new_table_version_found!= 0) {
		uint8_t version_number;
		psi_section_ctx_t *psi_section_ctx_nth;
		uint16_t pid= proc_ctx->proc_instance_index;

		/* Release previous stored table and update with the new one */
		pthread_mutex_lock(psi_table_ctx_mutex_p);
		psi_table_ctx_release(&psi_table_proc_ctx->psi_table_ctx);
		psi_table_proc_ctx->psi_table_ctx= psi_table_ctx_dup(psi_table_ctx);
		pthread_mutex_unlock(psi_table_ctx_mutex_p);
		CHECK_DO(psi_table_proc_ctx->psi_table_ctx!= NULL, goto end);

		/* Trace the new table type and PID */
		psi_section_ctx_nth= (psi_section_ctx_t*)llist_get_nth(
				psi_table_ctx->psi_section_ctx_llist, 0);
		version_number= psi_section_ctx_nth->version_number;
		LOGW("New %s table parsed: PID= %u (0x%0x); version %u (0x%0x)\n",
				pid== 0? "PAT": "PSI",
						pid, pid, version_number, version_number);
	}

	end_code= STAT_SUCCESS;
end:
	if(psi_table_ctx!= NULL)
		psi_table_ctx_release(&psi_table_ctx);
	return end_code;
}

/**
 * Implements the proc_if_s::opt callback.
 * See .proc_if.h for further details.
 */
static int psi_table_proc_opt(proc_ctx_t *proc_ctx, const char *tag,
		va_list arg)
{
	int end_code= STAT_ERROR;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(tag!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	if(TAG_IS("PROCS_ID_GET_CSTRUCT_REST")) {
		end_code= psi_table_proc_rest_get(proc_ctx, PROC_IF_REST_FMT_BINARY,
				va_arg(arg, void**));
	} else {
		LOGE("Unknown option\n");
		end_code= STAT_ENOTFOUND;
	}
	return end_code;
}

/**
 * Implements the proc_if_s::rest_get callback.
 * See .proc_if.h for further details.
 * NOTE: will be used indirectly by calling 'psi_table_proc_opt()'.
 */
static int psi_table_proc_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse)
{
	int end_code= STAT_ERROR;
	psi_table_proc_ctx_t *psi_table_proc_ctx= NULL;  // Do no release (alias)
	psi_proc_ctx_t *psi_proc_ctx= NULL; // Do not release (alias)
	pthread_mutex_t *psi_table_ctx_mutex_p= NULL; // Do no release
	psi_table_ctx_t *psi_table_ctx_ret= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(rest_fmt< PROC_IF_REST_FMT_ENUM_MAX, return STAT_ERROR);
	CHECK_DO(ref_reponse!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	*ref_reponse= NULL;

	/* Get PSI table processor context structure and related fields */
	psi_table_proc_ctx= (psi_table_proc_ctx_t*)proc_ctx;
	psi_proc_ctx= (psi_proc_ctx_t*)proc_ctx;
	psi_table_ctx_mutex_p= &psi_proc_ctx->psi_opaque_ctx_mutex;

	/* **** Attach data to REST response **** */

	/* PSI table */
	pthread_mutex_lock(psi_table_ctx_mutex_p);
	if(psi_table_proc_ctx->psi_table_ctx!= NULL)
		psi_table_ctx_ret= psi_table_ctx_dup(psi_table_proc_ctx->psi_table_ctx);
	pthread_mutex_unlock(psi_table_ctx_mutex_p);

	// Reserved for future use: set other data values here...

	/* Format response to be returned */
	switch(rest_fmt) {
	case PROC_IF_REST_FMT_BINARY:
		*ref_reponse= (void*)psi_table_ctx_ret;
		psi_table_ctx_ret= NULL; // Avoid double referencing
		break;
	default:
		LOGE("Unknown format requested for processor REST\n");
		goto end;
	}

	end_code= STAT_SUCCESS;
end:
	if(psi_table_ctx_ret!= NULL)
		psi_table_ctx_release(&psi_table_ctx_ret);
	return end_code;
}

/* **** PSI section processor **** */

/**
 * Implements the proc_if_s::open callback.
 * See .proc_if.h for further details.
 */
static proc_ctx_t* psi_section_proc_open(const proc_if_t *proc_if,
		const char *settings_str, const char* href, log_ctx_t *log_ctx,
		va_list arg)
{
	int ret_code, end_code= STAT_ERROR;
	psi_section_proc_ctx_t *psi_section_proc_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(proc_if!= NULL, return NULL);
	CHECK_DO(settings_str!= NULL, return NULL);
	// Parameter 'href' is allowed to be NULL
	// Parameter 'log_ctx' is allowed to be NULL

	/* Allocate context structure */
	psi_section_proc_ctx= (psi_section_proc_ctx_t*)calloc(1, sizeof(
			psi_section_proc_ctx_t));
	CHECK_DO(psi_section_proc_ctx!= NULL, goto end);

	/* **** Initialize context structure **** */

	/* Initialize PSI processors common structure */
	ret_code= psi_proc_ctx_init((psi_proc_ctx_t*)psi_section_proc_ctx, proc_if,
			settings_str, LOG_CTX_GET(), arg);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	psi_section_proc_ctx->psi_section_ctx= NULL;

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		psi_section_proc_close((proc_ctx_t**)&psi_section_proc_ctx);
	return (proc_ctx_t*)psi_section_proc_ctx;
}

/**
 * Implements the proc_if_s::close callback.
 * See .proc_if.h for further details.
 */
static void psi_section_proc_close(proc_ctx_t **ref_proc_ctx)
{
	psi_section_proc_ctx_t *psi_section_proc_ctx;
	//LOG_CTX_INIT(NULL);

	if(ref_proc_ctx== NULL ||
			(psi_section_proc_ctx= (psi_section_proc_ctx_t*)*ref_proc_ctx)
			== NULL)
		return;

	//LOG_CTX_SET(((proc_ctx_t*)psi_section_proc_ctx)->log_ctx);

	/* De-initialize PSI processors common structure */
	psi_proc_ctx_deinit((psi_proc_ctx_t*)psi_section_proc_ctx);

	/* Release parsed section */
	psi_section_ctx_release(&psi_section_proc_ctx->psi_section_ctx);

	/* Release context structure */
	free(psi_section_proc_ctx);
	*ref_proc_ctx= NULL;
}

/**
 * Implements the proc_if_s::process_frame callback.
 * See .proc_if.h for further details.
 */
static int psi_section_proc_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t* iput_fifo_ctx, fifo_ctx_t* oput_fifo_ctx)
{
	uint16_t pid;
	int new_table_version_found, ret_code, end_code= STAT_ERROR;
	psi_section_proc_ctx_t *psi_section_proc_ctx=
			NULL; // Do not release (alias)
	psi_proc_ctx_t *psi_proc_ctx= NULL; // Do not release (alias)
	pthread_mutex_t *psi_section_ctx_mutex_p= NULL; // Do not release
	psi_section_ctx_t *psi_section_ctx= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(iput_fifo_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(oput_fifo_ctx!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	pid= proc_ctx->proc_instance_index;

	/* Get PSI section processor context structure and related fields */
	psi_section_proc_ctx= (psi_section_proc_ctx_t*)proc_ctx;
	psi_proc_ctx= (psi_proc_ctx_t*)proc_ctx;
	psi_section_ctx_mutex_p= &psi_proc_ctx->psi_opaque_ctx_mutex;

	/* Get next PSI section from input stream */
	ret_code= psi_dec_get_next_section(iput_fifo_ctx, LOG_CTX_GET(),
			&psi_proc_ctx->tscc_input, pid, &psi_section_ctx);
	if(ret_code!= STAT_SUCCESS) {
		end_code= ret_code;
		goto end;
	}
	CHECK_DO(psi_section_ctx!= NULL, goto end);

	/* **** Update stored table copy **** */

	/* Check if table has changed */
	new_table_version_found= psi_section_proc_ctx->psi_section_ctx== NULL ||
			psi_section_ctx_cmp(psi_section_ctx,
					psi_section_proc_ctx->psi_section_ctx)!= 0;

	/* If new table version found, store into context
	 * (manage context members in mutual exclusion)
	 */
	if(new_table_version_found!= 0) {
		uint8_t version_number;

		/* Release previous stored section and update with the new one */
		pthread_mutex_lock(psi_section_ctx_mutex_p);
		psi_section_ctx_release(&psi_section_proc_ctx->psi_section_ctx);
		psi_section_proc_ctx->psi_section_ctx= psi_section_ctx_dup(
				psi_section_ctx);
		pthread_mutex_unlock(psi_section_ctx_mutex_p);
		CHECK_DO(psi_section_proc_ctx->psi_section_ctx!= NULL, goto end);

		/* Trace the new section version and PID */
		version_number= psi_section_ctx->version_number;
		LOGW("New PSI-section parsed: PID= %u (0x%0x); version %u (0x%0x)\n",
				pid, pid, version_number, version_number);
	}

	end_code= STAT_SUCCESS;
end:
	if(psi_section_ctx!= NULL)
		psi_section_ctx_release(&psi_section_ctx);
	return end_code;
}

/**
 * Implements the proc_if_s::opt callback.
 * See .proc_if.h for further details.
 */
static int psi_section_proc_opt(proc_ctx_t *proc_ctx, const char *tag,
		va_list arg)
{
	int end_code= STAT_ERROR;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(tag!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	if(TAG_IS("PROCS_ID_GET_CSTRUCT_REST")) {
		end_code= psi_section_proc_rest_get(proc_ctx, PROC_IF_REST_FMT_BINARY,
				va_arg(arg, void**));
	} else {
		LOGE("Unknown option\n");
		end_code= STAT_ENOTFOUND;
	}
	return end_code;
}

/**
 * Implements the proc_if_s::rest_get callback.
 * See .proc_if.h for further details.
 */
static int psi_section_proc_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse)
{
	int end_code= STAT_ERROR;
	psi_section_proc_ctx_t *psi_section_proc_ctx=
			NULL; // Do not release (alias)
	psi_proc_ctx_t *psi_proc_ctx= NULL; // Do not release (alias)
	pthread_mutex_t *psi_section_ctx_mutex_p= NULL; // Do no release
	psi_section_ctx_t *psi_section_ctx_ret= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(rest_fmt< PROC_IF_REST_FMT_ENUM_MAX, return STAT_ERROR);
	CHECK_DO(ref_reponse!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	*ref_reponse= NULL;

	/* Get PSI section processor context structure and related fields */
	psi_section_proc_ctx= (psi_section_proc_ctx_t*)proc_ctx;
	psi_proc_ctx= (psi_proc_ctx_t*)proc_ctx;
	psi_section_ctx_mutex_p= &psi_proc_ctx->psi_opaque_ctx_mutex;

	/* **** Attach data to REST response **** */

	/* PSI section */
	pthread_mutex_lock(psi_section_ctx_mutex_p);
	if(psi_section_proc_ctx->psi_section_ctx!= NULL)
		psi_section_ctx_ret= psi_section_ctx_dup(
				psi_section_proc_ctx->psi_section_ctx);
	pthread_mutex_unlock(psi_section_ctx_mutex_p);

	// Reserved for future use: set other data values here...

	/* Format response to be returned */
	switch(rest_fmt) {
	case PROC_IF_REST_FMT_BINARY:
		*ref_reponse= (void*)psi_section_ctx_ret;
		psi_section_ctx_ret= NULL; // Avoid double referencing
		break;
	default:
		LOGE("Unknown format requested for processor REST\n");
		goto end;
	}

	end_code= STAT_SUCCESS;
end:
	if(psi_section_ctx_ret!= NULL)
		psi_section_ctx_release(&psi_section_ctx_ret);
	return end_code;
}
