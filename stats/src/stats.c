/*
 * Copyright (c) 2015 Rafael Antoniello
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
 * @file stats.c
 * @author Rafael Antoniello
 */

#include "stats.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>
#include <pthread.h>
#include <string.h>

#include <libcjson/cJSON.h>

#define LOG_CTX_DEFULT
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/interr_usleep.h>
#include <libmediaprocsutils/schedule.h>
#include "proc_stat_cpu.h"
#include "proc_net_dev.h"
#include "getRSS.h"

/* **** Definitions **** */

#define STATS_THR_PERIOD (1000*1000) // [usecs] units

typedef struct net_dev_ctx_s { //TODO: add descriptions
	char *name, *bitrate_rx, *bitrate_tx;
} net_dev_ctx_t;

/**
 * Module's private context structure.
 */
typedef struct stats_module_ctx_s { //TODO: add descriptions
	volatile int flag_exit;
	interr_usleep_ctx_t *interr_usleep_ctx;
	char *cpu_usage[PROC_STAT_NUM_CPU_MAX][STATS_WINDOW_SECS];
	uint8_t cpu_num;
	net_dev_ctx_t net_dev_stats[PROC_NET_DEV_MAX_DEV_NUM][STATS_WINDOW_SECS];
	uint8_t net_dev_num;
	size_t maxrss;
	size_t currss;
	pthread_t stats_thread[STATS_ID_MAX];
	pthread_mutex_t mutex[STATS_ID_MAX];
} stats_module_ctx_t;

/* **** Prototypes **** */

static char* stats_cpu_get();
static char* stats_net_get();
static char* stats_rss_get();

static void* stats_cpu_thr(void* t);
static void* stats_net_thr(void* t);
static void* stats_rss_thr(void* t);

static void stats_cpu_update_globals(stats_module_ctx_t *stats_module_ctx,
		char **proc_stat_cpu_array);
static void stats_net_update_globals(stats_module_ctx_t *stats_module_ctx,
		char **proc_net_dev_array);
static void stats_rss_update_globals(stats_module_ctx_t *stats_module_ctx);

/* **** Implementations **** */

static stats_module_ctx_t *stats_module_ctx= NULL;

int stats_module_open()
{
	int cpu_num, net_dev_num;
	int  i, j, ret_code, end_code= STAT_ERROR;

	/* Check module initialization */
	if(stats_module_ctx!= NULL) {
		LOGE("'STATS' module is already initialized\n");
		return STAT_ERROR;
	}

	stats_module_ctx= (stats_module_ctx_t*)calloc(1, sizeof(
			stats_module_ctx_t));
	CHECK_DO(stats_module_ctx!= NULL, goto end);

	/* **** Initialize members **** */

	stats_module_ctx->interr_usleep_ctx= interr_usleep_open();
	CHECK_DO(stats_module_ctx->interr_usleep_ctx!= NULL, goto end);

	cpu_num= proc_stat_cpu_get_num_cpus();
	for(i= 0; i< cpu_num; i++) {
		for(j= 0; j< STATS_WINDOW_SECS; j++)
			stats_module_ctx->cpu_usage[i][j]= strdup("0");
	}
	stats_module_ctx->cpu_num= cpu_num;
	//LOGV("cpu_num: %u\n", stats_module_ctx->cpu_num); //Comment-me

	net_dev_num= proc_net_dev_get_num_devs();
	for(i= 0; i< net_dev_num; i++) {
		for(j= 0; j< STATS_WINDOW_SECS; j++) {
			stats_module_ctx->net_dev_stats[i][j].name= strdup("");
			stats_module_ctx->net_dev_stats[i][j].bitrate_rx= strdup("0");
			stats_module_ctx->net_dev_stats[i][j].bitrate_tx= strdup("0");
		}
	}
	stats_module_ctx->net_dev_num= net_dev_num;
	//LOGV("net_dev_num: %u\n", stats_module_ctx->net_dev_num); //Comment-me

	for(i= 0; i< STATS_ID_MAX; i++) {
		ret_code= pthread_mutex_init(&stats_module_ctx->mutex[i], NULL);
		CHECK_DO(ret_code== 0, goto end);
	}

	/* Launch statistics reporting threads */
	pthread_create(&stats_module_ctx->stats_thread[STATS_CPU], NULL,
			stats_cpu_thr, (void*)stats_module_ctx);
	pthread_create(&stats_module_ctx->stats_thread[STATS_NETWORK], NULL,
			stats_net_thr, (void*)stats_module_ctx);
	pthread_create(&stats_module_ctx->stats_thread[STATS_RSS], NULL,
			stats_rss_thr, (void*)stats_module_ctx);

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		stats_module_close();
	return end_code;
}

void stats_module_close()
{
	int i, j;

	/* Check module initialization */
	if(stats_module_ctx== NULL)
		return;

	/* Join statistics reporting thread */
	stats_module_ctx->flag_exit= 1;
	interr_usleep_unblock(stats_module_ctx->interr_usleep_ctx);
	for(i= 0; i< STATS_ID_MAX; i++) {
		//LOGV("Waiting STATS thread[%d] to join...\n", i);
		pthread_join(stats_module_ctx->stats_thread[i], NULL);
		//LOGV("Thread joined O.K.\n");
	}

	/* Release member 'cpu_usage' (array of strings) */
	for(i= 0; i< PROC_STAT_NUM_CPU_MAX; i++) {
		for(j= 0; j< STATS_WINDOW_SECS; j++) {
			if(stats_module_ctx->cpu_usage[i][j]!= NULL) {
				free(stats_module_ctx->cpu_usage[i][j]);
				stats_module_ctx->cpu_usage[i][j]= NULL;
			}
		}
	}

	/* Release member 'net_dev_stats' (array of strings) */
	for(i= 0; i< PROC_NET_DEV_MAX_DEV_NUM; i++) {
		for(j= 0; j< STATS_WINDOW_SECS; j++) {
			if(stats_module_ctx->net_dev_stats[i][j].name!= NULL)
				free(stats_module_ctx->net_dev_stats[i][j].name);
			if(stats_module_ctx->net_dev_stats[i][j].bitrate_rx!= NULL)
				free(stats_module_ctx->net_dev_stats[i][j].bitrate_rx);
			if(stats_module_ctx->net_dev_stats[i][j].bitrate_tx!= NULL)
				free(stats_module_ctx->net_dev_stats[i][j].bitrate_tx);
		}
	}

	/* Release 'interruptible usleep' module instance */
	interr_usleep_close(&stats_module_ctx->interr_usleep_ctx);

	/* Release mutex */
	for(i= 0; i< STATS_ID_MAX; i++)
		pthread_mutex_destroy(&stats_module_ctx->mutex[i]);

	free(stats_module_ctx);
	stats_module_ctx= NULL;
}

char* stats_module_get(stats_id_t id)
{
	char *ret= NULL;

	/* Check module initialization */
	if(stats_module_ctx== NULL) {
		LOGE("'STATS' module should be initialized previously\n");
		return NULL;
	}

	switch(id) {
	case STATS_CPU:
		pthread_mutex_lock(&stats_module_ctx->mutex[STATS_CPU]);
		ret= stats_cpu_get();
		pthread_mutex_unlock(&stats_module_ctx->mutex[STATS_CPU]);
		break;
	case STATS_NETWORK:
		pthread_mutex_lock(&stats_module_ctx->mutex[STATS_NETWORK]);
		ret= stats_net_get();
		pthread_mutex_unlock(&stats_module_ctx->mutex[STATS_NETWORK]);
		break;
	case STATS_RSS:
		pthread_mutex_lock(&stats_module_ctx->mutex[STATS_RSS]);
		ret= stats_rss_get();
		pthread_mutex_unlock(&stats_module_ctx->mutex[STATS_RSS]);
		break;
	default:
		LOGW("Uknown operation id requested\n");
		break;
	}
	return ret;
}

static char* stats_cpu_get()
{
	char *ret;
	cJSON *root, *cpu_stats;
	int i, j;
	uint8_t cpu_num;

	CHECK_DO(stats_module_ctx!= NULL, NULL);
	cpu_num= stats_module_ctx->cpu_num;

	/* Serialize 'cpu_usage' to a JSON string to be returned */
	root= cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "cpu_number", cpu_num);
	cJSON_AddNumberToObject(root, "time_window", STATS_WINDOW_SECS);
	cJSON_AddItemToObject(root, "cpu_stats", cpu_stats= cJSON_CreateArray());
	for(i= 0; i< cpu_num; i++) {
		cJSON *cpu_stats_obj, *data;
		char label[16]= {"CPU average"};
		if(i!= 0) snprintf(&label[4], 12, "%d", i);
		cJSON_AddItemToArray(cpu_stats, cpu_stats_obj= cJSON_CreateObject());
		cJSON_AddItemToObject(cpu_stats_obj, "label",
				cJSON_CreateString(label));
		cJSON_AddItemToObject(cpu_stats_obj, "data", data= cJSON_CreateArray());
		for(j= STATS_WINDOW_SECS- 1; j>= 0; j--) {
			cJSON *xy;
			char *cpu_usage= stats_module_ctx->cpu_usage[i][j];
			CHECK_DO(cpu_usage!= NULL, continue);
			cJSON_AddItemToArray(data, xy= cJSON_CreateArray());
			cJSON_AddItemToArray(xy, cJSON_CreateNumber((double)j));
			cJSON_AddItemToArray(xy, cJSON_CreateNumber(atoll(cpu_usage)));
		}
	}
	ret= cJSON_Print(root);
	cJSON_Delete(root);
	return ret;
}

static char* stats_net_get()
{
	char *ret;
	cJSON *root, *net_stats;
	int i, j;
	uint8_t net_dev_num;

	CHECK_DO(stats_module_ctx!= NULL, NULL);
	net_dev_num= stats_module_ctx->net_dev_num;

	/* Serialize 'net_dev_stats' to a JSON string to be returned */
	root= cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "dev_number", net_dev_num);
	cJSON_AddNumberToObject(root, "time_window", STATS_WINDOW_SECS);
	cJSON_AddItemToObject(root, "net_stats", net_stats= cJSON_CreateArray());
	for(i= 0; i< net_dev_num; i++) {
		cJSON *net_stats_obj, *data;
		char label[16]= {0};
		char* dev_name= stats_module_ctx->net_dev_stats[i][0].name;
		CHECK_DO(dev_name!= NULL, continue);

		/* Write Rx Json series for this device */
		snprintf(label, sizeof(label), "%s Rx", dev_name);
		cJSON_AddItemToArray(net_stats, net_stats_obj= cJSON_CreateObject());
		cJSON_AddItemToObject(net_stats_obj, "label",
				cJSON_CreateString(label));
		cJSON_AddItemToObject(net_stats_obj, "data", data= cJSON_CreateArray());
		for(j= STATS_WINDOW_SECS- 1; j>= 0; j--) {
			cJSON *xy;
			char* bitrate_rx= stats_module_ctx->net_dev_stats[i][j].bitrate_rx;
			CHECK_DO(bitrate_rx!= NULL, continue);
			cJSON_AddItemToArray(data, xy= cJSON_CreateArray());
			cJSON_AddItemToArray(xy, cJSON_CreateNumber((double)j));
			cJSON_AddItemToArray(xy, cJSON_CreateNumber(atoll(bitrate_rx)));
		}

		/* Write Tx Json series for this device */
		snprintf(label, sizeof(label), "%s Tx", dev_name);
		cJSON_AddItemToArray(net_stats, net_stats_obj= cJSON_CreateObject());
		cJSON_AddItemToObject(net_stats_obj, "label",
				cJSON_CreateString(label));
		cJSON_AddItemToObject(net_stats_obj, "data", data= cJSON_CreateArray());
		for(j= STATS_WINDOW_SECS- 1; j>= 0; j--) {
			cJSON *xy;
			char* bitrate_tx= stats_module_ctx->net_dev_stats[i][j].bitrate_tx;
			CHECK_DO(bitrate_tx!= NULL, continue);
			cJSON_AddItemToArray(data, xy= cJSON_CreateArray());
			cJSON_AddItemToArray(xy, cJSON_CreateNumber((double)j));
			cJSON_AddItemToArray(xy, cJSON_CreateNumber(atoll(bitrate_tx)));
		}
	}
	ret= cJSON_Print(root);
	cJSON_Delete(root);
	return ret;
}

static char* stats_rss_get()
{
	char *ret;
	cJSON *root;

	CHECK_DO(stats_module_ctx!= NULL, NULL);

	/* Serialize 'cpu_usage' to a JSON string to be returned */
	root= cJSON_CreateObject();
	CHECK_DO(root!= NULL, return NULL);
	cJSON_AddNumberToObject(root, "maxrss", stats_module_ctx->maxrss);
	cJSON_AddNumberToObject(root, "currss", stats_module_ctx->currss);
	ret= cJSON_Print(root);
	cJSON_Delete(root);
	return ret;
}

static void* stats_cpu_thr(void* t)
{
	stats_module_ctx_t *stats_module_ctx= (stats_module_ctx_t*)t;

	/* Check arguments */
	CHECK_DO(stats_module_ctx!= NULL, return (void*)(intptr_t)STAT_ERROR);

	while(stats_module_ctx->flag_exit== 0) {
		char **proc_stat_cpu_array;

		/* Get last second CPU statistics.
		 * NOTE: If succeed, 'proc_stat_cpu_get()' sleeps internally.
		 */
		if((proc_stat_cpu_array= proc_stat_cpu_get(
				stats_module_ctx->interr_usleep_ctx))== NULL) {
			schedule(); // schedule to avoid closed loops
			continue;
		}

		/* Update CPU statistics global members */
		stats_cpu_update_globals(stats_module_ctx, proc_stat_cpu_array);

		/* Release CPU statistics context structure */
		proc_stat_cpu_release(&proc_stat_cpu_array);
	}
	return (void*)(intptr_t)STAT_SUCCESS;
}

static void* stats_net_thr(void* t)
{
	stats_module_ctx_t *stats_module_ctx= (stats_module_ctx_t*)t;

	/* Check arguments */
	CHECK_DO(stats_module_ctx!= NULL, return (void*)(intptr_t)STAT_ERROR);

	while(stats_module_ctx->flag_exit== 0) {
		char **proc_net_dev_array;

		/* Get Network statistics of the last second.
		 * NOTE: If succeed, 'proc_net_dev_get()' sleeps internally.
		 */
		if((proc_net_dev_array= proc_net_dev_get(
				stats_module_ctx->interr_usleep_ctx))== NULL) {
			schedule(); // schedule to avoid closed loops
			continue;
		}

		/* Update Network statistics global members */
		stats_net_update_globals(stats_module_ctx, proc_net_dev_array);

		/* Release Network statistics context structure */
		proc_net_dev_release(&proc_net_dev_array);
	}
	return (void*)(intptr_t)STAT_SUCCESS;
}

static void* stats_rss_thr(void* t)
{
	stats_module_ctx_t *stats_module_ctx= (stats_module_ctx_t*)t;

	/* Check arguments */
	CHECK_DO(stats_module_ctx!= NULL, return (void*)(intptr_t)STAT_ERROR);

	while(stats_module_ctx->flag_exit== 0) {
		int ret_code;

		/* Update RSS statistics global members */
		stats_rss_update_globals(stats_module_ctx);

		ret_code= interr_usleep(stats_module_ctx->interr_usleep_ctx,
				STATS_THR_PERIOD);
		ASSERT(ret_code== STAT_SUCCESS || ret_code== STAT_EINTR);
	}
	return (void*)(intptr_t)STAT_SUCCESS;
}

static void stats_cpu_update_globals(stats_module_ctx_t *stats_module_ctx,
		char **proc_stat_cpu_array)
{
	int i, j;

	/* Check arguments */
	CHECK_DO(stats_module_ctx!= NULL, return);
	CHECK_DO(proc_stat_cpu_array!= NULL, return);

	pthread_mutex_lock(&stats_module_ctx->mutex[STATS_CPU]);

	/* Release "oldest" value, shift rest of "old" values circularly, and
	 * update current value */
	for(i= 0; i< stats_module_ctx->cpu_num; i++) {
		const int stat_window= STATS_WINDOW_SECS;

		/* Release "oldest" value */
		if(stats_module_ctx->cpu_usage[i][stat_window- 1]!= NULL) {
			free(stats_module_ctx->cpu_usage[i][stat_window- 1]);
			stats_module_ctx->cpu_usage[i][stat_window- 1]= NULL;
		}

		/* Shift rest of "old" values */
		for(j= stat_window- 1; j> 0; j--)
			stats_module_ctx->cpu_usage[i][j]=
					stats_module_ctx->cpu_usage[i][j- 1];

		/* Update current value */
		stats_module_ctx->cpu_usage[i][0]= strdup(proc_stat_cpu_array[i]);
	}

#if 0 // Comment-me
	/* Trace 'cpu_usage' global array */
	for(i= 0; i< stats_module_ctx->cpu_num; i++) {
		for(j= 0; j< stat_window; j++) {
			LOG("%s ", stats_module_ctx->cpu_usage[i][j]);
		}
		LOG("\n");
	}
	LOG("\n");
#endif

	pthread_mutex_unlock(&stats_module_ctx->mutex[STATS_CPU]);
}

static void stats_net_update_globals(stats_module_ctx_t *stats_module_ctx,
		char **proc_net_dev_array)
{
	int i, j;

	/* Check arguments */
	CHECK_DO(stats_module_ctx!= NULL, return);
	CHECK_DO(proc_net_dev_array!= NULL, return);

	pthread_mutex_lock(&stats_module_ctx->mutex[STATS_NETWORK]);

	/* Release "oldest" value, shift rest of "old" values circularly, and
	 * update current value */
	for(i= 0; proc_net_dev_array[i]!= NULL; i+= PROC_NET_DEV_PARAMS_PER_TAG) {
		const int stat_window= STATS_WINDOW_SECS;
		const int dev_idx= i/PROC_NET_DEV_PARAMS_PER_TAG;
		net_dev_ctx_t *net_dev_ctx_oldest=
				&stats_module_ctx->net_dev_stats[dev_idx][stat_window- 1];

		/* Release "oldest" value */
		if(net_dev_ctx_oldest->name!= NULL) {
			free(net_dev_ctx_oldest->name);
			net_dev_ctx_oldest->name= NULL;
		}
		if(net_dev_ctx_oldest->bitrate_rx!= NULL) {
			free(net_dev_ctx_oldest->bitrate_rx);
			net_dev_ctx_oldest->bitrate_rx= NULL;
		}
		if(net_dev_ctx_oldest->bitrate_tx!= NULL) {
			free(net_dev_ctx_oldest->bitrate_tx);
			net_dev_ctx_oldest->bitrate_tx= NULL;
		}

		/* Shift rest of "old" values */
		for(j= stat_window- 1; j> 0; j--)
			stats_module_ctx->net_dev_stats[dev_idx][j]=
					stats_module_ctx->net_dev_stats[dev_idx][j- 1];

		/* Update current value */
		stats_module_ctx->net_dev_stats[dev_idx][0].name=
				strdup(proc_net_dev_array[i]);
		stats_module_ctx->net_dev_stats[dev_idx][0].bitrate_rx=
				strdup(proc_net_dev_array[i+ 1]);
		stats_module_ctx->net_dev_stats[dev_idx][0].bitrate_tx=
				strdup(proc_net_dev_array[i+ 2]);
	}

	/* Update number of network devices */
	stats_module_ctx->net_dev_num= i/PROC_NET_DEV_PARAMS_PER_TAG;

#if 0 // Comment-me
	/* Trace 'net_dev_stats' global array */
	for(i= 0; i< stats_module_ctx->net_dev_num; i++) {
		for(j= 0; j< STATS_WINDOW_SECS; j++) {
			LOG("%s %s %s\n", stats_module_ctx->net_dev_stats[i][j].name,
					stats_module_ctx->net_dev_stats[i][j].bitrate_rx,
					stats_module_ctx->net_dev_stats[i][j].bitrate_tx);
		}
	}
#endif

	pthread_mutex_unlock(&stats_module_ctx->mutex[STATS_NETWORK]);
}

static void stats_rss_update_globals(stats_module_ctx_t *stats_module_ctx)
{
	/* Check arguments */
	CHECK_DO(stats_module_ctx!= NULL, return);

	pthread_mutex_lock(&stats_module_ctx->mutex[STATS_RSS]);
	stats_module_ctx->maxrss= getPeakRSS();
	stats_module_ctx->currss= getCurrentRSS();
	pthread_mutex_unlock(&stats_module_ctx->mutex[STATS_RSS]);
}
