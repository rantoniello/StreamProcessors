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
 * @file stream_procs.c
 * @Author Rafael Antoniello
 */

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <errno.h>

#include <libconfig.h>
#include <mongoose-5.3.h>

#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libstreamprocsstats/stats.h>
#include <libmediaprocsutils/comm.h>
#include <libmediaprocs/procs.h>
#include <libstreamprocsmpeg2ts/mpeg2_sp.h>
#include "stream_procs_api_http.h"

/* **** Definitions **** */

/** Daemonize this program */
//#define DAEMONIZE

/** Maximum number of stream processors instances permitted */
#define STREAM_PROCS_MAX_INSTANCES 64

/** Project directory complete path */
#ifndef _BASE_PATH //HACK
#define _BASE_PATH "./"
#endif

/** Installation directory complete path */
#ifndef _INSTALL_DIR //HACK: "fake" path for IDE
#define _INSTALL_DIR "./"
#endif

/** Stream Processors HTML directory root complete path */
#define PATH_STR_HTML _BASE_PATH"/assets/html"

/** Stream Processors PID file complete path */
#define PATH_PID_FILE _INSTALL_DIR"/var/run/"_PROCNAME".pid"

/** Stream Processors library configuration file directory complete path */
#ifndef _CONFIG_FILE_DIR //HACK: "fake" path for IDE
#define _CONFIG_FILE_DIR "./"
#endif

typedef struct http_server_param_ctx_s {
	procs_ctx_t *procs_ctx;
} http_server_param_ctx_t;

/* **** Prototypes **** */

static void stream_procs_quit_signal_handler();
static void stream_procs_run_server();
static int stream_procs_http_event_handler(struct mg_connection *conn,
		enum mg_event ev);

/* **** Implementations **** */

static volatile int flag_app_exit= 0;

int main(int argc, char **argv)
{
	int pid_file_fd, ret_code;
	ssize_t written;
	size_t pid_str_len;
	sigset_t set;
	char pid_str[16]= {0};
#ifdef DAEMONIZE
	pid_t pid= 0, sid= 0; // process ID and Session ID
#endif
	LOG_CTX_INIT(NULL);

	/* First of all, open essential LOG module */
	if(log_module_open()!= STAT_SUCCESS)
		exit(EXIT_FAILURE);

//	/* Treat arguments */
//	if(argc!= 1) {
//		stream_procs_usage();
//		exit(EXIT_FAILURE);
//	}

	/* **** DAEMONIZE **** */
#ifdef DAEMONIZE

	/* Fork off the parent process */
	pid= fork();
	if(pid< 0) {
		LOGE("Could not fork process to create daemon\n");
		exit(EXIT_FAILURE);
	}

	/* If we got a good PID, then we can exit the parent process */
	if(pid> 0) {
		//parent task code
		LOGV("Application daemon is running...\n");
		exit(EXIT_SUCCESS);
	}

	/* **** WE ARE NOW EXECUTING CHILD CODE **** */

	/* Change the file mode mask in order to write to any files (including
	 * logs) created by the daemon.
	 */
	umask(0);

	/* Create a unique new session ID for the child process; otherwise, the
	 * child process becomes an orphan in the system. Note that 'setsid()'
	 * creates a new session whose session ID is the same as the PID of
	 * the process that called 'setsid()'.
	 */
	if((sid= setsid())< 0) {
		LOGE("Could not create a new SID for the process\n");
		exit(EXIT_FAILURE);
	}

	/* Change the current working directory:
	 * The current working directory should be changed to some place that is
	 * guaranteed to always be there.
	 */
	//if ((chdir("/"))< 0) { // Does not apply
	//	LOGE("Could not change directory for main task\n");
	//	exit(EXIT_FAILURE);
	//}

	/* Close out the standard file descriptors since a daemon cannot use the
	 * terminal.
	 */
	//close(STDIN_FILENO); // Does not apply
	//close(STDOUT_FILENO);
	//close(STDERR_FILENO);

#endif // DAEMONIZE

	/* Set SIGNAL handlers to this process */
	sigfillset(&set);
	sigdelset(&set, SIGINT);
	pthread_sigmask(SIG_SETMASK, &set, NULL);
	signal(SIGINT, stream_procs_quit_signal_handler);
	//signal(SIGQUIT, stream_procs_quit_signal_handler);
	//signal(SIGTERM, stream_procs_quit_signal_handler);

	/* Write (create/truncate) PID-file for this process */
	pid_file_fd= open(PATH_PID_FILE, O_RDWR| O_APPEND| O_TRUNC| O_CREAT,
			S_IRUSR| S_IWUSR);
    if(pid_file_fd< 0) {
        LOGE("Unable to create PID-file '%s'.\n", PATH_PID_FILE);
        exit(EXIT_FAILURE);
    }
    snprintf(pid_str, sizeof(pid_str), "%d", (int)getpid());
    written= write(pid_file_fd, pid_str, (pid_str_len= strlen(pid_str)));
    CHECK_DO(written== (ssize_t)pid_str_len, exit(EXIT_FAILURE));
    close(pid_file_fd);

    /* Initialize all other system modules */
    ret_code= stats_module_open();
    CHECK_DO(ret_code== STAT_SUCCESS, exit(EXIT_FAILURE));

	ret_code= comm_module_open(LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, exit(EXIT_FAILURE));

	ret_code= procs_module_open(NULL);
	CHECK_DO(ret_code== STAT_SUCCESS, exit(EXIT_FAILURE));

	ret_code= procs_module_opt("PROCS_REGISTER_TYPE", &proc_if_mpeg2_sp);
	CHECK_DO(ret_code== STAT_SUCCESS, exit(EXIT_FAILURE));

	/* Create and run server */
	stream_procs_run_server();

	/* Close system modules */
	stats_module_close();
	comm_module_close();
	procs_module_close();

	/* Delete PID-file */
	unlink(PATH_PID_FILE);

	LOGV("Stream processor application finalized.\n");

	/* Finally, close essential LOG module */
	log_module_close();

	exit(EXIT_SUCCESS);
}

static void stream_procs_quit_signal_handler()
{
	LOG_CTX_INIT(NULL);
	LOGV("signaling application to finalize...\n");
	flag_app_exit= 1;
}

static void stream_procs_run_server()
{
	config_t cfg;
	const char *listening_port; // Do not release
	const char *host_ipv4_addr; // Do not release
	struct mg_server *http_server_ctx= NULL;
	int flag_cfg_init= 0; // means 'false'
	procs_ctx_t *procs_ctx= NULL;
	http_server_param_ctx_t http_server_param_ctx= {0};
	char host_ipv4_addr_port[128]= {0};
	LOG_CTX_INIT(NULL);

	/* Initialize configuration file management module */
	config_init(&cfg);
	flag_cfg_init= 1;

	/* Read configuration file. If there is an error, report it and exit. */
	if(!config_read_file(&cfg, _CONFIG_FILE_DIR))
	{
		LOGE("%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg),
				config_error_text(&cfg));
		goto end;
	}

	/* Get necessary configuration information.
	 * IMPORTANT NOTE: All pointers to configuration resources are static and
	 * should not be freed. Configuration file management context should be
	 * freed at the end.
	 */
	if(!config_lookup_string(&cfg, "http_server.listening_port",
			&listening_port)) {
		LOGE("Listening port *MUST* be specified in configuration file\n");
		goto end;
	}
	LOGV("Setting listening port: %s\n", listening_port);
	if(!config_lookup_string(&cfg, "http_server.local_address",
			&host_ipv4_addr)) {
		LOGE("Local address *MUST* be specified in configuration file\n");
		goto end;
	}

	/* Get PROCS module's instance */
	snprintf(host_ipv4_addr_port, sizeof(host_ipv4_addr_port), "%s:%s%s",
			host_ipv4_addr, listening_port, API_HTTP_BASE_URL);
	procs_ctx= procs_open(LOG_CTX_GET(), STREAM_PROCS_MAX_INSTANCES,
			"stream_procs", host_ipv4_addr_port);
	CHECK_DO(procs_ctx!= NULL, goto end);
	http_server_param_ctx.procs_ctx= procs_ctx;

	/* Create and configure the server */
	http_server_ctx= mg_create_server(&http_server_param_ctx,
			stream_procs_http_event_handler);
	mg_set_option(http_server_ctx, "listening_port", listening_port);
	mg_set_option(http_server_ctx, "document_root", PATH_STR_HTML);
	LOGV("Setting document root: '%s'\n", PATH_STR_HTML);

	/* Release configuration file module context */
	config_destroy(&cfg);
	flag_cfg_init= 0;

	/* Server main processing loop */
	LOGV("Starting server...\n");
	while(flag_app_exit== 0)
		mg_poll_server(http_server_ctx, 1000);

end:
	if(http_server_ctx!= NULL)
		mg_destroy_server(&http_server_ctx);
	if(flag_cfg_init!= 0) {
		config_destroy(&cfg);
		flag_cfg_init= 0;
	}
	if(procs_ctx!= NULL)
		procs_close(&procs_ctx);
	return;
}

static int stream_procs_http_event_handler(struct mg_connection *conn,
		enum mg_event ev)
{
	int ret_code;
	char *response= NULL;
	http_server_param_ctx_t *http_server_param_ctx= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(conn!= NULL, return MG_FALSE);

	http_server_param_ctx= (http_server_param_ctx_t*)conn->server_param;
	CHECK_DO(http_server_param_ctx!= NULL, return MG_FALSE);

	switch(ev){
	case MG_REQUEST:
		if(conn->uri!= NULL && strstr(conn->uri, "/api/")!= NULL) {
			/* Requesting API */
			ret_code= stream_procs_api_http_req_handler(
					http_server_param_ctx->procs_ctx, conn->uri,
					conn->query_string, conn->request_method, conn->content,
					conn->content_len, &response);
			if(response!= NULL) {
				mg_printf_data(conn, "%s", response);
				free(response);
				ret_code= MG_TRUE;
			} else {
				ret_code= MG_FALSE;
			}
		} else {
			ret_code= MG_FALSE; // Not API; response by default
		}
		break;
	case MG_AUTH:
		ret_code= MG_TRUE;
		break;
	default:
		ret_code= MG_FALSE;
		break;
	}
	return ret_code;
}
