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
 * @brief MPEG2 Stream Processor (SP) module unit-testing
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
#include <pthread.h>

#include <libcjson/cJSON.h>

#define ENABLE_DEBUG_LOGS //uncomment to trace logs
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/comm.h>
#include <libmediaprocsutils/tsudpsend.h>
#include <libdbdriver/dbdriver.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocs/procs.h>
#include <libmediaprocs/proc.h>
#include <libstreamprocsmpeg2ts/mpeg2_sp.h>
}

SUITE(UTESTS_MPEG2_SP)
{
	void* streaming_thr(void *t)
	{
		volatile int *ref_flag_exit= (volatile int*)t;
		char *argv[5]= {
				(char*)"tsudpsend",
				(char*)"./san_diego_200kbps.ts",
				(char*)"127.0.0.1",
				(char*)"2000",
				(char*)"200000"
		};
		tsudpsend(5, argv, ref_flag_exit);
		return NULL;
	}

	TEST(SIMPLE_TEST)
	{
		pthread_t streaming_thread;
		int ret_code, proc_id= -1;
		procs_ctx_t *procs_ctx= NULL;
		char *rest_str= NULL;
		cJSON *cjson_rest= NULL, *cjson_aux= NULL;
		volatile int flag_exit= 0;
		mongod_wrapper_ctx_t *mongod_wrapper_ctx= NULL;
		LOG_CTX_INIT(NULL);

		LOGV("\n\nExecuting UTESTS_MPEG2_SP::SIMPLE_TEST...\n");

		/* Launch mongod service */
		mongod_wrapper_ctx= mongod_wrapper_open(NULL, NULL,
				"mongodb://127.0.0.1:27017", "sp-use-example",
				"db_stream_procs", "coll_stream_procs", LOG_CTX_GET());
		CHECK_DO(mongod_wrapper_ctx!= NULL, CHECK(false); goto end);

		/* Set working directory */
		ret_code= chdir(_BASE_PATH"/mpeg2ts/utests/assets/");
		CHECK_DO(ret_code== 0, CHECK(false); goto end);

		ret_code= log_module_open();
		CHECK_DO(ret_code== STAT_SUCCESS, CHECK(false); goto end);

		ret_code= comm_module_open(NULL);
		CHECK_DO(ret_code== STAT_SUCCESS, CHECK(false); goto end);

		ret_code= procs_module_open(NULL);
		CHECK_DO(ret_code== STAT_SUCCESS, CHECK(false); goto end);

		ret_code= procs_module_opt("PROCS_REGISTER_TYPE", &proc_if_mpeg2_sp);
		CHECK_DO(ret_code== STAT_SUCCESS, CHECK(false); goto end);

		/* Get PROCS module's instance */
		procs_ctx= procs_open(NULL, 16, "stream_procs", NULL);
		CHECK_DO(procs_ctx!= NULL, CHECK(false); goto end);

		/* Get MPEG2 SP instance */
		ret_code= procs_opt(procs_ctx, "PROCS_POST", "mpeg2_sp", "", &rest_str);
		CHECK(ret_code== STAT_SUCCESS && rest_str!= NULL);
		if(rest_str!= NULL) {
			printf("PROCS_POST: '%s'\n", rest_str); //comment-me
			fflush(stdout); //comment-me
			cjson_rest= cJSON_Parse(rest_str);
			CHECK_DO(cjson_rest!= NULL, CHECK(false); goto end);
			cjson_aux= cJSON_GetObjectItem(cjson_rest, "proc_id");
			CHECK_DO(cjson_aux!= NULL, CHECK(false); goto end);
			CHECK((proc_id= cjson_aux->valuedouble)>= 0);
			free(rest_str);
			rest_str= NULL;
		}

		/* Launch streaming thread */
		ret_code= pthread_create(&streaming_thread, NULL, streaming_thr,
				(void*)&flag_exit);
		CHECK_DO(ret_code== 0, CHECK(false); goto end);

		/* Set input URL */
		LOGV("Putting new URL: 'udp://127.0.0.1:2000'... \n");
		ret_code= procs_opt(procs_ctx, "PROCS_ID_PUT", proc_id,
				"input_url=udp://127.0.0.1:2000");
		CHECK(ret_code== STAT_SUCCESS);

		/* Get REST */
		LOGV("Getting stream processor REST... \n");
		ret_code= procs_opt(procs_ctx, "PROCS_ID_GET", proc_id, &rest_str);
		CHECK_DO(ret_code== STAT_SUCCESS && rest_str!= NULL,
				CHECK(false); goto end);
		LOGV("Response:\n'%s'\n", rest_str);
		free(rest_str);
		rest_str= NULL;
		usleep(1000* 1000* 15);

		/* Reset input URL to "NULL" */
		LOGV("Putting new URL: '' -NULL-... \n");
		ret_code= procs_opt(procs_ctx, "PROCS_ID_PUT", proc_id, "input_url=");
		CHECK(ret_code== STAT_SUCCESS);
		ret_code= procs_opt(procs_ctx, "PROCS_ID_GET", proc_id, &rest_str);
		CHECK_DO(ret_code== STAT_SUCCESS && rest_str!= NULL,
				CHECK(false); goto end);
		LOGV("Response:\n'%s'\n", rest_str);
		free(rest_str);
		rest_str= NULL;
		usleep(1000* 1000* 5);

		/* Reset input URL to "non input" */
		LOGV("Putting new URL: 'udp://127.0.0.1:4000'... \n");
		ret_code= procs_opt(procs_ctx, "PROCS_ID_PUT", proc_id,
				"input_url=udp://127.0.0.1:4000");
		CHECK(ret_code== STAT_SUCCESS);
		ret_code= procs_opt(procs_ctx, "PROCS_ID_GET", proc_id, &rest_str);
		CHECK_DO(ret_code== STAT_SUCCESS && rest_str!= NULL,
				CHECK(false); goto end);
		LOGV("Response:\n'%s'\n", rest_str);
		free(rest_str);
		rest_str= NULL;
		usleep(1000* 1000* 5);

		/* Reset input URL to original value */
		LOGV("Putting new URL: 'udp://127.0.0.1:2000'... \n");
		ret_code= procs_opt(procs_ctx, "PROCS_ID_PUT", proc_id,
				"input_url=udp://127.0.0.1:2000");
		CHECK(ret_code== STAT_SUCCESS);
		ret_code= procs_opt(procs_ctx, "PROCS_ID_GET", proc_id, &rest_str);
		CHECK_DO(ret_code== STAT_SUCCESS && rest_str!= NULL,
				CHECK(false); goto end);
		LOGV("Response:\n'%s'\n", rest_str);
		free(rest_str);
		rest_str= NULL;
		usleep(1000* 1000* 5);

		/* Get REST */
		LOGV("Getting stream processor REST... \n");
		ret_code= procs_opt(procs_ctx, "PROCS_ID_GET", proc_id, &rest_str);
		CHECK_DO(ret_code== STAT_SUCCESS && rest_str!= NULL,
				CHECK(false); goto end);
		LOGV("Response:\n'%s'\n", rest_str);
		free(rest_str);
		rest_str= NULL;

		/* Join streaming thread */
		flag_exit= 1;
		pthread_join(streaming_thread, NULL);

		/* Release MPEG2 SP instance */
		ret_code= procs_opt(procs_ctx, "PROCS_ID_DELETE", proc_id);
		CHECK(ret_code== STAT_SUCCESS);

end:
		if(procs_ctx!= NULL)
			procs_close(&procs_ctx);
		procs_module_close();
		if(rest_str!= NULL)
			free(rest_str);
		if(cjson_rest!= NULL)
			cJSON_Delete(cjson_rest);
		mongod_wrapper_close(&mongod_wrapper_ctx, LOG_CTX_GET());
		comm_module_close();
		log_module_close();

		LOGV("... passed O.K.\n");
	}
}
