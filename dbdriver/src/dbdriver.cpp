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
 * @file dbdriver.c
 * @author Rafael Antoniello
 */

#include "dbdriver.h"

extern "C" {
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
}

#include <mongoc.h>

extern "C" {
#define ENABLE_DEBUG_LOGS //uncomment to trace logs
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/interr_usleep.h>
}

/* **** Definitions **** */

/** Installation directory complete path */
#ifndef _INSTALL_DIR //HACK: "fake" path for IDE
#define _INSTALL_DIR "./"
#endif

/**
 * Module's instance context structure.
 */
typedef struct mongod_wrapper_ctx_s {
	/**
	 * Children (forked) process Id.
	 */
	volatile int process_pid;
	/**
	 * Interruptible u-sleep to use in case the module's instance is closed by
	 * the calling application.
	 */
	interr_usleep_ctx_t *interr_usleep_ctx;
	/**
	 * Exit indicator.
	 * Set to non-zero to indicate to abort immediately.
	 */
	volatile int flag_exit;
} mongod_wrapper_ctx_t;

/* **** Prototypes **** */

/* **** Implementations **** */

/**
 * Default arguments for executing 'mongod' service.
 */
static char *const argv_def[]= {
		_INSTALL_DIR"/bin/mongod",
		"-f",
		_INSTALL_DIR"/etc/mongod/mongod.conf",
		NULL
};

/**
 * Default environment variables for executing 'mongod' service.
 */
static char *const envp_def[] = {
		"LD_LIBRARY_PATH="_INSTALL_DIR"/lib",
		NULL
};

mongod_wrapper_ctx_t* mongod_wrapper_open(char *const argv[],
		char *const envp[], const char *mongod_uri, log_ctx_t *log_ctx)
{
	pid_t cpid;
	char *const *ref_argv, *const *ref_envp;
	int i, ret_code, end_code= STAT_ERROR;
	mongod_wrapper_ctx_t *mongod_wrapper_ctx= NULL;
	char *str= NULL;
	bson_error_t bson_error= {0};
	bson_t *bson_command= NULL;
	bson_t bson_reply= BSON_INITIALIZER; // -has to be this horrible way-
	mongoc_client_t *client= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	// Parameter 'argv' is allowed to be NULL
	// Parameter 'envp' is allowed to be NULL
	CHECK_DO(mongod_uri!= NULL, return NULL);
	// Parameter 'flag_mongoc_is_init' not necessary to check
	// Parameter 'log_ctx' is allowed to be NULL
	if(argv== NULL)
		ref_argv= argv_def;
	else
		ref_argv= argv;
	if(envp== NULL)
		ref_envp= envp_def;
	else
		ref_envp= envp;

	/* Fork process */
	cpid= fork();
	CHECK_DO(cpid>= 0, goto end);

	if(cpid== 0) {
		/* **** Code executed by child **** */
		LOGD("\nmongod process starting PID is %ld.\n", (long)getpid());

#ifdef ENABLE_DEBUG_LOGS
		LOGD("Executing mongod as: '%s ", ref_argv[0]);
		for(i= 1; ref_argv[i]!= NULL; i++)
			LOGD("%s ", ref_argv[i]);
		if(ref_envp!= NULL) {
			for(i= 0; ref_envp[i]!= NULL; i++)
				LOGD("%s ", ref_envp[i]);
		}
		LOGD("\n");
#endif

		//The exec() functions return only if an error has occurred.
		// The return value is -1, and errno is set to indicate the error.
		if((ret_code= execve(ref_argv[0], ref_argv, ref_envp))< 0) {
			ASSERT(0);
			exit(ret_code);
		}
		// This point is unreachable
	}

	/* **** Code executed by parent **** */

	/* Allocate context structure */
	mongod_wrapper_ctx= (mongod_wrapper_ctx_t*)calloc(1,
			sizeof(mongod_wrapper_ctx_t));
	CHECK_DO(mongod_wrapper_ctx!= NULL, goto end);

	/* **** Initialize context structure **** */

	mongod_wrapper_ctx->process_pid= cpid;

	mongod_wrapper_ctx->interr_usleep_ctx= interr_usleep_open();
	CHECK_DO(mongod_wrapper_ctx->interr_usleep_ctx!= NULL, goto end);

	mongod_wrapper_ctx->flag_exit= 0;

	/* Initialize libmongoc's internals */
	mongoc_init();

	/* Create a new client instance */
	client= mongoc_client_new(mongod_uri);
	CHECK_DO(client!= NULL, goto end);

	/* Ping/wait the database and print the result as JSON */
#define WAIT_100MSECS (10* 30) // 30 seconds...
	LOGD("Waiting mongod to start");
	for(i= 0, ret_code= 0; i< WAIT_100MSECS; i++) {

		ret_code= interr_usleep(mongod_wrapper_ctx->interr_usleep_ctx,
				1000* 100); // 100msecs
		ASSERT(ret_code!= STAT_ERROR);

		if(bson_command!= NULL)
			bson_destroy(bson_command);
		bson_command= BCON_NEW("ping", BCON_INT32(1));
		bson_destroy(&bson_reply); // horrible but necessary
		ret_code= mongoc_client_command_simple(client,
				"admin", bson_command, NULL, &bson_reply, &bson_error);
		if(ret_code!= 0) {
			LOGD("... O.K.\n");
			break;
		} else {
			LOGD(".");
		}
	}
	if(ret_code== 0) {
		LOGE("Mongoc PING command failed: '%s'\n", bson_error.message);
		goto end;
	}
	str= bson_as_json(&bson_reply, NULL);
	CHECK_DO(str!= NULL, goto end);
	LOGD("PING reply: '%s'\n", str);

	end_code= STAT_SUCCESS;
end:
	if(client!= NULL)
		mongoc_client_destroy(client);
	if(bson_command!= NULL)
		bson_destroy(bson_command);
	bson_destroy(&bson_reply);
	if(str!= NULL)
		bson_free(str);
	if(end_code!= STAT_SUCCESS)
		mongod_wrapper_close(&mongod_wrapper_ctx, LOG_CTX_GET());
	return mongod_wrapper_ctx;
}

void mongod_wrapper_close(mongod_wrapper_ctx_t **ref_mongod_wrapper_ctx,
		log_ctx_t *log_ctx)
{
	pid_t cpid, w;
	int ret_code, status;
	mongod_wrapper_ctx_t *mongod_wrapper_ctx;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	if(ref_mongod_wrapper_ctx== NULL ||
			(mongod_wrapper_ctx= *ref_mongod_wrapper_ctx)== NULL)
		return;
	// Parameter 'log_ctx' is allowed to be NULL

	/* Get process PID */
	cpid= mongod_wrapper_ctx->process_pid;
	CHECK_DO(cpid>= 0, return);

	/* Signal mongod to quit gracefully */
	LOGD("\nSignaling mongod to exit (read PID= %d)\n", cpid);
	mongod_wrapper_ctx->flag_exit= 1;
	if(mongod_wrapper_ctx->interr_usleep_ctx!= NULL)
		interr_usleep_unblock(mongod_wrapper_ctx->interr_usleep_ctx);
	ASSERT((ret_code= kill(cpid, SIGQUIT))== 0);

	/* Wait mongod to finalize */
	do {
		w= waitpid(cpid, &status, WUNTRACED| WCONTINUED);
		if(w== -1) {
			perror("\nwaitpid\n");
			exit(-1);
		}
		if(WIFEXITED(status)) {
			LOGD("\nmongod exited with status= %d\n", WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			LOGD("\nmongod killed by signal %d\n", WTERMSIG(status));
		} else if (WIFSTOPPED(status)) {
			LOGD("\nmongod stopped by signal %d\n", WSTOPSIG(status));
		} else if (WIFCONTINUED(status)) {
			LOGD("\nmongod continued\n");
		}
	} while(!WIFEXITED(status) && !WIFSIGNALED(status));

	interr_usleep_close(&mongod_wrapper_ctx->interr_usleep_ctx);

	mongoc_cleanup();

	/* Release context structure */
	free(mongod_wrapper_ctx);
	ref_mongod_wrapper_ctx= NULL;
}
