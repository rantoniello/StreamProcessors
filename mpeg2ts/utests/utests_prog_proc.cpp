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
 * @file utests_prog_proc.cpp
 * @brief program processor module unit-testing
 * @author Rafael Antoniello
 */

#include <UnitTest++/UnitTest++.h>

extern "C" {
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <libcjson/cJSON.h>
#include <libmediaprocsutils/uri_parser.h>
#include <libmbedtls_base64/base64.h>

#define ENABLE_DEBUG_LOGS //uncomment to trace logs
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocsutils/llist.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocs/procs.h>
#include <libmediaprocs/proc.h>
#include <libstreamprocsmpeg2ts/ts.h>
#include <libstreamprocsmpeg2ts/ts_enc.h>
#include <libstreamprocsmpeg2ts/prog_proc.h>
#include <libstreamprocsmpeg2ts/psi.h>
#include <libstreamprocsmpeg2ts/psi_enc.h>
}

#define PMT_PID 99
#define PCR_PID 100
#define ES1_PID 100
#define NUMBER_OF_FRAMES_IN_TEST 10
#define FPS_IN_TEST 2

TEST(PROGRAM_PROC_POST_DELETE)
{
	int i, ret_code, proc_id= -1;
	procs_ctx_t *procs_ctx= NULL;
	char *rest_str= NULL;
	cJSON *cjson_rest= NULL, *cjson_aux= NULL;
	uint8_t anydata[184]= {};
	proc_frame_ctx_t proc_frame_ctx= {0};
	psi_pms_es_ctx_t *psi_pms_es_ctx=
			NULL; // released within 'psi_section_ctx_release()'
	psi_pms_ctx_t *psi_pms_ctx=
			NULL; // released within 'psi_section_ctx_release()'
	psi_section_ctx_t *psi_section_ctx= NULL;
	void *psi_buf;
	size_t psi_buf_size;
	int end_code= STAT_ERROR;
	const char *settings_fmt=  "{"
		 "\"output_url\":\"234.5.5.5:2000\","
		 "\"selected_brctrl_type_value\":0,"
		 "\"cbr\":10000,"
		 "\"flag_clear_input_bitrate_peak\":false,"
		 "\"flag_purge_disassociated_processors\":false,"
		 "\"pmt_octet_stream\":\"%s\","
		 "\"max_ts_pcr_guard_msec\":200,"
		 "\"min_stc_delay_output_msec\":200"
		 "}";
	const int settings_buf_len= 4096;
	char settings_buf[settings_buf_len];
	size_t olen_base64;
	unsigned char *dst_base64= NULL;
	void *ts_buf= NULL;
	size_t ts_buf_size= 0;
	LOG_CTX_INIT(NULL);

	/* Open LOG module */
	ret_code= log_module_open();
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Open PROCS module */
	ret_code= procs_module_open(NULL);
	CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_NOTMODIFIED, goto end);

	/* Register MPEG2-TS program processor */
	ret_code= procs_module_opt("PROCS_REGISTER_TYPE", &proc_if_mpeg2_prog_proc);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Get PROCS module's instance */
	LOGD("Opening program-processor module instance...\n");
	procs_ctx= procs_open(NULL, 16, NULL, NULL);
	CHECK_DO(procs_ctx!= NULL, goto end);

	/* **** Compose program-processor settings string **** */

	/* Create PMS-ES (to be encoded below) */
	psi_pms_es_ctx= psi_pms_es_ctx_allocate();
	CHECK_DO(psi_pms_es_ctx!= NULL, goto end);
	psi_pms_es_ctx->stream_type= 0;
	psi_pms_es_ctx->elementary_PID= ES1_PID;
	psi_pms_es_ctx->es_info_length= 0;
	psi_pms_es_ctx->psi_desc_ctx_llist= NULL;

	/* Create PMS (to be encoded below) */
	psi_pms_ctx= psi_pms_ctx_allocate();
	CHECK_DO(psi_pms_ctx!= NULL, goto end);
	psi_pms_ctx->pcr_pid= PCR_PID;
	psi_pms_ctx->program_info_length= 0;
	psi_pms_ctx->psi_desc_ctx_llist= NULL;
	psi_pms_ctx->psi_pms_es_ctx_llist= NULL;
	ret_code= llist_push(&psi_pms_ctx->psi_pms_es_ctx_llist, psi_pms_es_ctx);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);
	psi_pms_ctx->crc_32= 0;

	/* Encode PSI section */
	psi_section_ctx= psi_section_ctx_allocate();
	CHECK_DO(psi_section_ctx!= NULL, goto end);
	psi_section_ctx->table_id= PSI_TABLE_TS_PROGRAM_MAP_SECTION;
	psi_section_ctx->section_syntax_indicator= 1;
	psi_section_ctx->indicator_1= 0;
	psi_section_ctx->reserved_1= 0;
	psi_section_ctx->section_length= 9+ (PSI_SECTION_FIXED_LEN- 3);
	psi_section_ctx->table_id_extension= 12;
	psi_section_ctx->reserved_2= 0;
	psi_section_ctx->version_number= 2;
	psi_section_ctx->current_next_indicator= 2;
	psi_section_ctx->section_number= 0;
	psi_section_ctx->last_section_number= 0;
	psi_section_ctx->data= psi_pms_ctx;
	psi_section_ctx->crc_32= 0; // computed within 'psi_section_ctx_enc()'
	ret_code= psi_section_ctx_enc(psi_section_ctx, PMT_PID, LOG_CTX_GET(),
			&psi_buf, &psi_buf_size);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Encode Program Map Section from binary to base64:
	 * - Get necessary buffer length in 'olen_base64' variable;
	 * - Allocate destination buffer;
	 * - Actually encode data to destination buffer.
	 */
	ret_code= mbedtls_base64_encode(dst_base64, 0 /* to get length */,
			&olen_base64, (const unsigned char*)psi_buf, psi_buf_size);
	CHECK_DO(ret_code== MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && olen_base64> 0,
			goto end);

	dst_base64= (unsigned char*)calloc(1, olen_base64+ 1);
	CHECK_DO(dst_base64!= NULL, goto end);

	ret_code= mbedtls_base64_encode(dst_base64, olen_base64, &olen_base64,
			(const unsigned char*)psi_buf, psi_buf_size);
	CHECK_DO(ret_code== 0, goto end);

	/* Compose settings string */
	CHECK_DO(snprintf(settings_buf, settings_buf_len, settings_fmt,
			dst_base64)< settings_buf_len, goto end);
	LOGD("Program-processor input settings: '%s'\n", settings_buf);

	/* Create program-processor instance */
	LOGD("Instantiating program-processor...\n");
	ret_code= procs_opt(procs_ctx, "PROCS_POST", "prog_proc", settings_buf,
			&rest_str);
	CHECK_DO(ret_code== STAT_SUCCESS && rest_str!= NULL, goto end);
	printf("PROCS_POST: '%s'\n", rest_str); fflush(stdout); // comment-me
	cjson_rest= cJSON_Parse(rest_str);
	CHECK_DO(cjson_rest!= NULL, goto end);
	cjson_aux= cJSON_GetObjectItem(cjson_rest, "proc_id");
	CHECK_DO(cjson_aux!= NULL && (proc_id= cjson_aux->valuedouble)>= 0,
			goto end);
	free(rest_str);
	rest_str= NULL;

	/* **** Operate program-processor **** */

	for(i= 0; i< NUMBER_OF_FRAMES_IN_TEST; i++) {
		ts_ctx_s ts_ctx= {
				0, //uint8_t transport_error_indicator;
				0, //uint8_t payload_unit_start_indicator;
				0, //uint8_t transport_priority;
				ES1_PID, //uint16_t pid;
				0, //uint8_t scrambling_control;
				0, //uint8_t adaptation_field_exist;
				1, //uint8_t contains_payload;
				(uint8_t)(i& (int)0x0F),//uint8_t continuity_counter;
				NULL, //ts_af_ctx_t *adaptation_field;
				anydata, //uint8_t *payload;
				sizeof(anydata) //uint8_t payload_size;
		};
		anydata[0]= i; // we edit the first byte as a kind of counter

		if(ts_buf!= NULL) {
			free(ts_buf);
			ts_buf= NULL;
			ts_buf_size= 0;
		}
		ret_code= ts_enc_packet(&ts_ctx, LOG_CTX_GET(), &ts_buf, &ts_buf_size);
		CHECK_DO(ret_code== STAT_SUCCESS, goto end);

		LOGD("Send frame...\n");
	    proc_frame_ctx.data= (uint8_t*)ts_buf;
	    proc_frame_ctx.p_data[0]= (uint8_t*)ts_buf;
	    proc_frame_ctx.linesize[0]= ts_buf_size;
	    proc_frame_ctx.width[0]= ts_buf_size;
	    proc_frame_ctx.height[0]= 1;
	    proc_frame_ctx.proc_sample_fmt= PROC_IF_FMT_UNDEF;
	    proc_frame_ctx.pts= -1;
	    proc_frame_ctx.dts= -1;
	    proc_frame_ctx.es_id= proc_id;
	    ret_code= procs_send_frame(procs_ctx, proc_id, &proc_frame_ctx);
	    CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	    usleep((1000* 1000)/ FPS_IN_TEST);
	}

	LOGD("Deleting program-processor instance.\n");
	ret_code= procs_opt(procs_ctx, "PROCS_ID_DELETE", proc_id);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	ret_code= procs_module_opt("PROCS_UNREGISTER_TYPE", "prog_proc");
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
	CHECK(end_code== STAT_SUCCESS);
	if(procs_ctx!= NULL)
		procs_close(&procs_ctx);
	procs_module_close();
	if(rest_str!= NULL)
		free(rest_str);
	if(cjson_rest!= NULL)
		cJSON_Delete(cjson_rest);
	psi_section_ctx_release(&psi_section_ctx);
	if(psi_buf!= NULL)
		free(psi_buf);
	if(dst_base64!= NULL)
		free(dst_base64);
	if(ts_buf!= NULL)
		free(ts_buf);
	log_module_close();
}
