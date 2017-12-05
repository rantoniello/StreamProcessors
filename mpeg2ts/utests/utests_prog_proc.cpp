/*
 * Copyright (c) 2017, 2018 Rafael Antoniello
 *
 * This file is part of MediaProcessors.
 *
 * MediaProcessors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MediaProcessors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MediaProcessors. If not, see <http://www.gnu.org/licenses/>.
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

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocs/procs.h>
#include <libmediaprocs/proc.h>
#include <libstreamprocsmpeg2ts/prog_proc.h>
}

#define ENABLE_DEBUG_LOGS //uncomment to trace logs
#ifdef ENABLE_DEBUG_LOGS
	#define LOGD_CTX_INIT(CTX) LOG_CTX_INIT(CTX)
	#define LOGD(FORMAT, ...) LOG(FORMAT, ##__VA_ARGS__)
#else
	#define LOGD_CTX_INIT(CTX)
	#define LOGD(...)
#endif

TEST(PROGRAM_PROC_POST_DELETE)
{
	int ret_code, proc_id= -1;
	procs_ctx_t *procs_ctx= NULL;
	char *rest_str= NULL;
	cJSON *cjson_rest= NULL, *cjson_aux= NULL;
	LOG_CTX_INIT(NULL);

	ret_code= log_module_open();
	CHECK_DO(ret_code== STAT_SUCCESS, CHECK(false); goto end);

	ret_code= procs_module_open(NULL);
	CHECK(ret_code== STAT_SUCCESS);

	ret_code= procs_module_opt("PROCS_REGISTER_TYPE", &proc_if_mpeg2_prog_proc);
	CHECK(ret_code== STAT_SUCCESS);

	/* Get PROCS module's instance */
	procs_ctx= procs_open(NULL);
	CHECK_DO(procs_ctx!= NULL, CHECK(false); goto end);

	ret_code= procs_opt(procs_ctx, "PROCS_POST", "prog_proc", "", &rest_str);
	CHECK_DO(ret_code== STAT_SUCCESS && rest_str!= NULL,
			CHECK(false); goto end);
	printf("PROCS_POST: '%s'\n", rest_str); fflush(stdout); // comment-me
	cjson_rest= cJSON_Parse(rest_str);
	CHECK_DO(cjson_rest!= NULL, CHECK(false); goto end);
	cjson_aux= cJSON_GetObjectItem(cjson_rest, "proc_id");
	CHECK_DO(cjson_aux!= NULL, CHECK(false); goto end);
	CHECK((proc_id= cjson_aux->valuedouble)>= 0);
	free(rest_str);
	rest_str= NULL;

	usleep(1000* 1000* 10);

	ret_code= procs_opt(procs_ctx, "PROCS_ID_DELETE", proc_id);
	CHECK(ret_code== STAT_SUCCESS);

	ret_code= procs_module_opt("PROCS_UNREGISTER_TYPE", "prog_proc");
	CHECK(ret_code== STAT_SUCCESS);

end:
	if(procs_ctx!= NULL)
		procs_close(&procs_ctx);
	procs_module_close();
	if(rest_str!= NULL)
		free(rest_str);
	if(cjson_rest!= NULL)
		cJSON_Delete(cjson_rest);
	log_module_close();
}
