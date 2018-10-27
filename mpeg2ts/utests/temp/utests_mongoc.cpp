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
 * @file utests_mongoc.cpp
 * @brief MongoDB-C-driver unit-testing
 * @author Rafael Antoniello
 */

#include <UnitTest++/UnitTest++.h>
#include <mongoc.h>

extern "C" {
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>

#include <libcjson/cJSON.h>

#define ENABLE_DEBUG_LOGS //uncomment to trace logs
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libdbdriver/dbdriver.h>
}

SUITE(UTESTS_MONGOC)
{
	/* In the StreamProcessors architecture, we will store the units settings
	 * (for example, program-processor settings, elementary-stream processor
	 * settings, ...) as documents of a single collection
	 * (e.g.: the "stream_procs_collection").
	 * The JSON strings "json_doc1" nad "json_doc2" characterize typical
	 * elementary-stream processor settings objects; "sys_id" field is
	 * unambiguous for the whole system level (we use the API URL of these
	 * objects as the system-id).
	 */
	const unsigned char json_doc1[]=
			"{"
				"\"sys_id\":\"sp-1-prog_proc-23-es_proc-4\","
				"\"settings\":{"
					"\"proc_name\":\"ffmpeg_m2v_enc\","
					"\"bit_rate_output\":307200,"
					"\"frame_rate_output\":15,"
					"\"width_output\":352,"
					"\"height_output\":288,"
					"\"gop_size\":15,"
					"\"conf_preset\":null"
				"}"
			"}";
	const unsigned char json_doc2[]=
			"{"
				"\"sys_id\":\"sp-1-prog_proc-2-es_proc-0\","
				"\"settings\":{"
					"\"proc_name\":\"ffmpeg_m2v_enc\","
					"\"bit_rate_output\":102400,"
					"\"frame_rate_output\":30,"
					"\"width_output\":352,"
					"\"height_output\":288,"
					"\"gop_size\":10,"
					"\"conf_preset\":null"
				"}"
			"}";

	//static void signals_handler(int sig_num)
	//{
	//	LOGD_CTX_INIT(NULL);
	//	LOGD("Signaling application; signal number: %d\n", sig_num);
	//}

	/* Find and print the requested documents of the given collection */
	static void mongoc_coll_print_doc(mongoc_collection_t *collection,
			bson_t *bson_query, bson_t *bson_opt, log_ctx_t *log_ctx)
	{
		bson_error_t error;
		const bson_t *bson_doc_iter; // Do not release
		mongoc_cursor_t *mongoc_cursor= NULL;
		char *str= NULL;
		LOG_CTX_INIT(log_ctx);

		/* Get collection cursor */
		mongoc_cursor= mongoc_collection_find_with_opts(collection,
				(const bson_t*)bson_query, bson_opt, NULL);
		CHECK_DO(mongoc_cursor!= NULL, goto end);

		/* Traverse the collection */
		while(mongoc_cursor_next(mongoc_cursor, &bson_doc_iter)!= 0) {
			if(str!= NULL)
				bson_free(str);
			str= bson_as_canonical_extended_json(bson_doc_iter, NULL);
			LOGD("Next document: '%s'\n", str);
		}
		/* Check if we iterated all the documents without errors */
		CHECK_DO(mongoc_cursor_error(mongoc_cursor, &error)== 0, goto end);

end:
		if(mongoc_cursor!= NULL) {
			mongoc_cursor_destroy(mongoc_cursor);
			mongoc_cursor= NULL;
		}
		if(str!= NULL)
			bson_free(str);
	}

	/* Print all the documents of the given collection */
	static void mongoc_coll_print_all_docs(mongoc_collection_t *collection,
			log_ctx_t *log_ctx)
	{
		// Pass '{}' empty-query to request 'all documents'
		bson_t bson_empty= BSON_INITIALIZER;
		mongoc_coll_print_doc(collection, &bson_empty, NULL, log_ctx);
		bson_destroy(&bson_empty);
	}
#if 0
	TEST(SIMPLE_TEST_MONGOD)
	{
		mongod_wrapper_ctx_t *mongod_wrapper_ctx= NULL;
		const char *mongod_uri= "mongodb://127.0.0.1:27017";
		LOG_CTX_INIT(NULL);

		mongod_wrapper_ctx= mongod_wrapper_open(NULL, NULL, mongod_uri,
				LOG_CTX_GET());
		CHECK(mongod_wrapper_ctx!= NULL);

		mongod_wrapper_close(&mongod_wrapper_ctx, LOG_CTX_GET());
	}
#endif
#if 1
	TEST(SIMPLE_TEST_CLIENT)
	{
		int ret_code;
		sigset_t set;
		bson_error_t bson_error;
		bson_t bson_reply= BSON_INITIALIZER; // -has to be this horrible way-
		bson_t bson_empty= BSON_INITIALIZER;
		mongoc_collection_t *collection= NULL;
		mongoc_client_t *client= NULL;
		mongoc_database_t *database= NULL;
		bson_t *bson_command= NULL, *bson_doc= NULL, *bson_query= NULL,
				*bson_opts= NULL;
		char *str= NULL;
		const char *mongod_uri= "mongodb://127.0.0.1:27017";
		mongod_wrapper_ctx_t *mongod_wrapper_ctx= NULL;
		LOG_CTX_INIT(NULL);

		LOGV("\n\nExecuting UTESTS_MONGOC::SIMPLE_TEST_CLIENT...\n");

		/* Launch mongod service.
		 * IMPORTANT NOTE: Remember 'mongoc_init()' is performed inside the
		 * wrapper 'mongod_wrapper_open()' (thus we do not need to call it).
		 * The function 'mongod_wrapper_close()' will finally call
		 * 'mongoc_cleanup()'.
		 */
		mongod_wrapper_ctx= mongod_wrapper_open(NULL, NULL, mongod_uri,
				LOG_CTX_GET());
		CHECK_DO(mongod_wrapper_ctx!= NULL, CHECK(false); goto end);

		/* Create a new client instance */
		client= mongoc_client_new(mongod_uri);
		CHECK_DO(client!= NULL, CHECK(false); goto end);

		/* Register the application name so we can track it in the profile logs
		 * on the server. This can also be done from the URI
		 * (see other examples).
		 */
		mongoc_client_set_appname(client, "sp-use-example");

		/* Get a handle on the database "db_stream_procs" and collection
		 * "coll_stream_procs"
		 */
		database= mongoc_client_get_database(client, "db_stream_procs");
		CHECK_DO(database!= NULL, CHECK(false); goto end);
		collection= mongoc_client_get_collection(client, "db_stream_procs",
				"coll_stream_procs");
		CHECK_DO(collection!= NULL, CHECK(false); goto end);

		/* Ping the database and print the result as JSON */
		bson_command= BCON_NEW("ping", BCON_INT32(1));
		bson_destroy(&bson_reply); // horrible but necessary
		ret_code= mongoc_client_command_simple(client, "admin", bson_command,
				NULL, &bson_reply, &bson_error);
		if(ret_code== 0) {
			CHECK(false);
			LOGE("Mongoc PING command failed: '%s'\n", bson_error.message);
			goto end;
		}
		str= bson_as_json(&bson_reply, NULL);
		CHECK_DO(str!= NULL, CHECK(false); goto end);
		LOGD("PING reply: '%s'\n", str);

		/* Conver our JSON documents into BSON; insert documents into
		 * collection.
		 */
		LOGD("\nInserting doc: '%s'\n", json_doc1);
		if(bson_doc!= NULL)
			bson_destroy(bson_doc);
		bson_doc= bson_new_from_json(json_doc1, -1, &bson_error);
		CHECK_DO(bson_doc!= NULL, CHECK(false); goto end);
		ret_code= mongoc_collection_insert(collection, MONGOC_INSERT_NONE,
				bson_doc, NULL, &bson_error);
		CHECK_DO(ret_code!= 0, CHECK(false); goto end);

		LOGD("Inserting doc: '%s'\n", json_doc2); //comment-me
		if(bson_doc!= NULL)
			bson_destroy(bson_doc);
		bson_doc= bson_new_from_json(json_doc2, -1, &bson_error);
		CHECK_DO(bson_doc!= NULL, CHECK(false); goto end);
		ret_code= mongoc_collection_insert(collection, MONGOC_INSERT_NONE,
				bson_doc, NULL, &bson_error);
		CHECK_DO(ret_code!= 0, CHECK(false); goto end);

		/* Trace full collection */
		LOGD("\nTracing the collection:\n");
		mongoc_coll_print_all_docs(collection, LOG_CTX_GET());

		/* Query a document; remove that specific document */
		LOGD("\nGet the document with sys-id 'sp-1-prog_proc-23-es_proc-4'\n");
		if(bson_query!= NULL)
			bson_destroy(bson_query);
		bson_query= BCON_NEW("sys_id",
				BCON_UTF8("sp-1-prog_proc-23-es_proc-4"));
		mongoc_coll_print_doc(collection, bson_query, bson_opts,
				LOG_CTX_GET());
		LOGD("\nRemove the document with sys-id "
				"'sp-1-prog_proc-23-es_proc-4'\n");
		ret_code= mongoc_collection_remove(collection, MONGOC_REMOVE_NONE,
				bson_query, NULL, &bson_error);
		CHECK(ret_code!= 0);

		/* Trace full collection */
		LOGD("\nTracing the collection:\n");
		mongoc_coll_print_all_docs(collection, LOG_CTX_GET());

		/* Set SIGNAL handlers to this process */
		//sigfillset(&set);
		//sigdelset(&set, SIGABRT); // Abort is used by "mongoc" when failing
		//pthread_sigmask(SIG_SETMASK, &set, NULL);
		//signal(SIGABRT, signals_handler);
		//bson_new_from_json(NULL, -1, &bson_error);
end:
		/* Erase (remove) all ('passing empty '{}' selector) our collection */
		if(collection!= NULL) {
			ret_code= mongoc_collection_remove(collection, MONGOC_REMOVE_NONE,
					&bson_empty, NULL, &bson_error);
			CHECK(ret_code!= 0);
			mongoc_collection_destroy(collection);
			collection= NULL;
		}
		if(database!= NULL)
			mongoc_database_destroy(database);
		if(client!= NULL)
			mongoc_client_destroy(client);
		if(bson_command!= NULL)
			bson_destroy(bson_command);
		if(str!= NULL)
			bson_free(str);
		if(bson_doc!= NULL)
			bson_destroy(bson_doc);
		if(bson_query!= NULL)
			bson_destroy(bson_query);
		if(bson_opts!= NULL)
			bson_destroy(bson_opts);
		bson_destroy(&bson_reply);
		bson_destroy(&bson_empty);

		mongod_wrapper_close(&mongod_wrapper_ctx, LOG_CTX_GET());

		LOGV("... passed O.K.\n");
	}
#endif
}
