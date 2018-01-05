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
 * @file stream_procs_docs.c
 * @brief Data Base driver for the Stream Processors library.
 * @author Rafael Antoniello
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>

#include <libcjson/cJSON.h>
#include <libconfig.h>
#include <mongoc.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>

/* **** Definitions **** */

//#define ENABLE_DEBUG_LOGS // Debugging purposes only
#ifdef ENABLE_DEBUG_LOGS
	#define LOGD_CTX_INIT(CTX) LOG_CTX_INIT(CTX)
	#define LOGD(FORMAT, ...) LOGV(FORMAT, ##__VA_ARGS__)
#else
	#define LOGD_CTX_INIT(CTX)
	#define LOGD(...)
#endif

/** Stream Processors library configuration file directory complete path */
#ifndef _CONFIG_FILE_DIR //HACK: "fake" path for IDE
#define _CONFIG_FILE_DIR "./"
#endif

/* Debugging purposes only */
//#define SIMULATE_FAIL_DB_UPDATE
#ifndef SIMULATE_FAIL_DB_UPDATE
	#define SIMULATE_CRASH()
#else
	#define SIMULATE_CRASH() exit(EXIT_FAILURE)
#endif

/* **** Prototypes **** */

static int stream_procs_docs_replace_json_doc(const char *doc_json_str,
		size_t doc_json_str_size, log_ctx_t *log_ctx);
static void mongoc_coll_delete_doc(mongoc_collection_t *collection,
		const char *sys_id, log_ctx_t *log_ctx);
static int mongoc_coll_insert_doc(mongoc_collection_t *collection,
		const char *sys_id, const char *doc_json_str, log_ctx_t *log_ctx);

/* **** Implementations **** */

int main(int argc, char *argv[], char *envp[])
{
	int end_code;
	size_t doc_json_str_size;
	char *doc_json_str= argv[1];
	LOG_CTX_INIT(NULL);
	SIMULATE_CRASH();

	log_module_open();

	/* Check arguments */
	if(argc!= 2) {
		printf("Usage: %s <doc-json-char-string-no-white-spaces>", argv[0]);
		exit(EXIT_FAILURE);
	}
	CHECK_DO(doc_json_str!= NULL, exit(EXIT_FAILURE));
	CHECK_DO((doc_json_str_size= strlen(doc_json_str))> 0, exit(EXIT_FAILURE));
	LOGD("argvc: %d; argv[0]: '%s'; argv[1]: '%s'\n", argc, argv[0],
			doc_json_str); //comment-me

	end_code= stream_procs_docs_replace_json_doc(doc_json_str,
			doc_json_str_size, LOG_CTX_GET());

	log_module_close();
	return (end_code== STAT_SUCCESS)? EXIT_SUCCESS: EXIT_FAILURE;
}

static int stream_procs_docs_replace_json_doc(const char *doc_json_str,
		size_t doc_json_str_size, log_ctx_t *log_ctx)
{
	config_t cfg;
	const char *config_p, *db_name; // Do not release
	int ret_code, end_code= STAT_ERROR;
	mongoc_client_t *client= NULL;
	mongoc_database_t *database= NULL;
	mongoc_collection_t *collection= NULL;
	cJSON *cjson_settings= NULL;
	cJSON *cjson_aux= NULL; // Do not release
	char *sys_id_p= NULL; // Do not release
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(doc_json_str!= NULL, return STAT_ERROR);
	CHECK_DO(doc_json_str_size> 0, return STAT_ERROR);

	/* **** We can assume we have new settings **** */

	/* Parse settings */
	cjson_settings= cJSON_Parse(doc_json_str);
	CHECK_DO(cjson_settings!= NULL, goto end);

	/* Get database configurations */
	config_init(&cfg);
	if(!config_read_file(&cfg, _CONFIG_FILE_DIR))
	{
		LOGE("%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg),
				config_error_text(&cfg));
		goto end;
	}
	if(!config_lookup_string(&cfg, "database.mongodb_uri", &config_p)) {
		LOGE("Local address *MUST* be specified in configuration file\n");
		goto end;
	}
	CHECK_DO(config_p!= NULL, goto end);

	/* Create a new client instance */
	client= mongoc_client_new(config_p);
	CHECK_DO(client!= NULL, goto end);

	/* Register the application name so we can track it in the profile logs
	 * on the server.
	 */
	if(!config_lookup_string(&cfg, "database.stream_procs.app_name",
			&config_p)) {
		LOGE("Stream processors application name *MUST* be specified in the "
				"database section at the configuration file\n");
		goto end;
	}
	CHECK_DO(config_p!= NULL, goto end);
	mongoc_client_set_appname(client, config_p);

	/* Get a handle on the database "db_stream_procs" and collection
	 * "coll_stream_procs"
	 */
	if(!config_lookup_string(&cfg, "database.stream_procs.db_name",
			&db_name)) {
		LOGE("Stream processors database name *MUST* be specified in the "
				"database section at the configuration file\n");
		goto end;
	}
	CHECK_DO(db_name!= NULL, goto end);
	database= mongoc_client_get_database(client, db_name);
	CHECK_DO(database!= NULL, goto end);

	if(!config_lookup_string(&cfg, "database.stream_procs.coll_name",
			&config_p)) {
		LOGE("Stream processors collection name *MUST* be specified in the "
				"database section at the configuration file\n");
		goto end;
	}
	CHECK_DO(config_p!= NULL, goto end);
	collection= mongoc_client_get_collection(client, db_name, config_p);
	CHECK_DO(collection!= NULL, goto end);

	/* Get document system Id. */
	cjson_aux= cJSON_GetObjectItem(cjson_settings, "sys_id");
	CHECK_DO(cjson_aux!= NULL, goto end);
	sys_id_p= cjson_aux->valuestring;
	CHECK_DO(sys_id_p!= NULL && strlen(sys_id_p)> 0, goto end);

	/* Delete corresponding document from database if it exist */
	mongoc_coll_delete_doc(collection, sys_id_p, LOG_CTX_GET());

	/* Insert new document in database */
	ret_code= mongoc_coll_insert_doc(collection, sys_id_p, doc_json_str,
			LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
	if(collection!= NULL)
		mongoc_collection_destroy(collection);
	if(database!= NULL)
		mongoc_database_destroy(database);
	if(client!= NULL)
		mongoc_client_destroy(client);
	if(cjson_settings!= NULL)
		cJSON_Delete(cjson_settings);
	return end_code;
}

/**
 * Delete requested document from database.
 * Find and delete the requested documents of the given collection.
 */
static void mongoc_coll_delete_doc(mongoc_collection_t *collection,
		const char *sys_id, log_ctx_t *log_ctx)
{
	int iter_cnt, ret_code, flag_doc_exist= 0;
	bson_error_t bson_error;
	const bson_t *bson_doc_iter; // Do not release
	bson_t *bson_query= NULL;
	mongoc_cursor_t *mongoc_cursor= NULL;
	LOG_CTX_INIT(log_ctx);

	/* CHeck argument */
	CHECK_DO(collection!= NULL, return);
	CHECK_DO(sys_id!= NULL, return);

	/* Compose query */
	bson_query= BCON_NEW("sys_id", BCON_UTF8(sys_id));
	CHECK_DO(bson_query!= NULL, goto end);

	/* Get collection cursor */
	mongoc_cursor= mongoc_collection_find_with_opts(collection,
			(const bson_t*)bson_query, NULL, NULL);
	CHECK_DO(mongoc_cursor!= NULL, goto end);

	/* Traverse the collection; check there should be maximum one document
	 * matching 'Sys. id.'. Delete document if it exist.
	 */
	iter_cnt= 0;
	while(mongoc_cursor_next(mongoc_cursor, &bson_doc_iter)!= 0 &&
			iter_cnt< 1) {
		if(bson_doc_iter!= NULL)
			flag_doc_exist= 1;
		iter_cnt++; // sanity check purposes
	}
	/* Check if we iterated all the documents without errors */
	CHECK_DO(mongoc_cursor_error(mongoc_cursor, &bson_error)== 0 &&
			iter_cnt<= 1, goto end);

	/* Remove document */
	if(flag_doc_exist) {
		LOGD("Remove the document with sys-id '%s'\n", sys_id); //comment-me
		ret_code= mongoc_collection_remove(collection, MONGOC_REMOVE_NONE,
				bson_query, NULL, &bson_error);
		CHECK_DO(ret_code!= 0, goto end);
		flag_doc_exist= 0;
	}

end:
	if(bson_query!= NULL)
		bson_destroy(bson_query);
	if(mongoc_cursor!= NULL)
		mongoc_cursor_destroy(mongoc_cursor);
}

static int mongoc_coll_insert_doc(mongoc_collection_t *collection,
		const char *sys_id, const char *doc_json_str, log_ctx_t *log_ctx)
{
	int iter_cnt, ret_code, end_code= STAT_ERROR;
	bson_error_t bson_error;
	const bson_t *bson_doc_iter; // Do not release
	bson_t *bson_query= NULL, *bson_doc= NULL, *bson_opt= NULL;
	mongoc_cursor_t *mongoc_cursor= NULL;
	//char *settings_mongo_str= NULL; //comment-me
	LOG_CTX_INIT(log_ctx);

	/* CHeck argument */
	CHECK_DO(collection!= NULL, return STAT_ERROR);
	CHECK_DO(sys_id!= NULL, return STAT_ERROR);
	CHECK_DO(doc_json_str!= NULL, return STAT_ERROR);

	/* Compose query */
	bson_query= BCON_NEW("sys_id", BCON_UTF8(sys_id));
	CHECK_DO(bson_query!= NULL, goto end);

	/* **** Sanity check: we will check if document exist ****
	 * - Get collection cursor;
	 * - Traverse the collection to check there is no matching document.
	 */
	mongoc_cursor= mongoc_collection_find_with_opts(collection,
			(const bson_t*)bson_query, NULL, NULL);
	CHECK_DO(mongoc_cursor!= NULL, goto end);
	iter_cnt= 0;
	while(mongoc_cursor_next(mongoc_cursor, &bson_doc_iter)!= 0 &&
			iter_cnt< 1) {
		iter_cnt++; // sanity check purposes
	}
	CHECK_DO(mongoc_cursor_error(mongoc_cursor, &bson_error)== 0 &&
			iter_cnt== 0, goto end);

	/* Insert new document */
	LOGD("Inserting the document with sys-id '%s'\n", sys_id); //comment-me
	bson_doc= bson_new_from_json((const uint8_t*)doc_json_str, -1, &bson_error);
	CHECK_DO(bson_doc!= NULL, goto end);
	ret_code= mongoc_collection_insert(collection, MONGOC_INSERT_NONE,
			bson_doc, NULL, &bson_error);
	CHECK_DO(ret_code!= 0, goto end);

	/* **** Sanity check: query for recently inserted document **** */
	if(mongoc_cursor!= NULL)
		mongoc_cursor_destroy(mongoc_cursor);
	if(bson_opt!= NULL)
		bson_destroy(bson_opt);
	bson_opt= BCON_NEW("projection", "{",
	                    "_id", BCON_BOOL (false), // Exclude "_id" field
	                 "}");
	mongoc_cursor= mongoc_collection_find_with_opts(collection,
			(const bson_t*)bson_query, (const bson_t*)bson_opt, NULL);
	CHECK_DO(mongoc_cursor!= NULL, goto end);
	iter_cnt= 0;
	while(mongoc_cursor_next(mongoc_cursor, &bson_doc_iter)!= 0 &&
			iter_cnt< 1) {
		if(bson_doc_iter!= NULL) {
			//if(settings_mongo_str!= NULL) //comment-me
			//	bson_free(settings_mongo_str); //comment-me
			//settings_mongo_str= bson_as_json(bson_doc_iter,
			//		NULL); //comment-me
			//LOGV("'%s'=='%s'?\n", doc_json_str,
			//		settings_mongo_str); //comment-me
			CHECK_DO(bson_compare(bson_doc, bson_doc_iter)== 0, goto end);
		}
		iter_cnt++; // sanity check purposes
	}

	/* Check if we iterated all the documents without errors */
	CHECK_DO(mongoc_cursor_error(mongoc_cursor, &bson_error)== 0 &&
			iter_cnt== 1, goto end);
	end_code= STAT_SUCCESS;
end:
	if(bson_query!= NULL)
		bson_destroy(bson_query);
	if(mongoc_cursor!= NULL)
		mongoc_cursor_destroy(mongoc_cursor);
	if(bson_doc!= NULL)
		bson_destroy(bson_doc);
	if(bson_opt!= NULL)
		bson_destroy(bson_opt);
	//if(settings_mongo_str!= NULL) //comment-me
	//	bson_free(settings_mongo_str); //comment-me
	return end_code;
}
