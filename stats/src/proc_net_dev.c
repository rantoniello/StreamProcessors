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
 * @file proc_net_dev.c
 * @Author Rafael Antoniello
 */

#include "proc_net_dev.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>

#define LOG_CTX_DEFULT
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/interr_usleep.h>

/* **** Definitions **** */

#define LINE_SIZE_MAX 1024
#define PROC_NET_DEV_FILE_PATH "/proc/net/dev"
#define PROC_NET_DEV_TAG_1 "eth"
#define PROC_NET_DEV_TAG_2 "wlan"
#define PROC_NET_DEV_TAG_3 "em"
#define PROC_NET_DEV_TAG_4 "en"
#define PROC_NET_DEV_TAG_5 "p"
#define PROC_NET_DEV_SIZEOFTAG_MAX 8
#define PROC_NET_DEV_MEASURE_PERIOD_USECS 1000*1000
#define NUM_CHAR_REPRESENTATION 32

/* **** Prototypes **** */

static int proc_net_dev_parse_lines(uint64_t rx[PROC_NET_DEV_MAX_DEV_NUM],
		uint64_t tx[PROC_NET_DEV_MAX_DEV_NUM],
		char devs[PROC_NET_DEV_MAX_DEV_NUM][PROC_NET_DEV_SIZEOFTAG_MAX]);

/* **** Implementations **** */

int proc_net_dev_get_num_devs()
{
	char line[LINE_SIZE_MAX];
	FILE *fp= NULL;
	int num_device= 0;

	fp= fopen(PROC_NET_DEV_FILE_PATH,"r");
	CHECK_DO(fp!= NULL, return STAT_ERROR);

	while(fgets(line, LINE_SIZE_MAX, fp)!= NULL &&
			num_device< PROC_NET_DEV_MAX_DEV_NUM) {
		/* Parse lines with network statistics */
		if(strstr(line, PROC_NET_DEV_TAG_1)!= NULL ||
		   strstr(line, PROC_NET_DEV_TAG_2)!= NULL ||
		   strstr(line, PROC_NET_DEV_TAG_3)!= NULL ||
		   strstr(line, PROC_NET_DEV_TAG_4)!= NULL ||
		   strstr(line, PROC_NET_DEV_TAG_5)!= NULL) {
			num_device++;
		}
	}
	fclose(fp);
	return num_device;
}

char** proc_net_dev_get(interr_usleep_ctx_t *interr_usleep_ctx)
{
	int i, j, num_devs= 0, ret_code, end_code= STAT_ERROR;
	char **ret_array= NULL;
	uint64_t rx1[PROC_NET_DEV_MAX_DEV_NUM]= {0},
			tx1[PROC_NET_DEV_MAX_DEV_NUM]= {0},
			rx2[PROC_NET_DEV_MAX_DEV_NUM]= {0},
			tx2[PROC_NET_DEV_MAX_DEV_NUM]= {0};
	char devs[PROC_NET_DEV_MAX_DEV_NUM][PROC_NET_DEV_SIZEOFTAG_MAX]= {{0}};

	/* Check arguments */
	CHECK_DO(interr_usleep_ctx!= NULL, goto end);

	num_devs= proc_net_dev_parse_lines(rx1, tx1, devs);
	CHECK_DO(num_devs> 0, goto end);

	ret_code= interr_usleep(interr_usleep_ctx,
			PROC_NET_DEV_MEASURE_PERIOD_USECS);
	if(ret_code== STAT_EINTR)
		goto end;
	ASSERT(ret_code== STAT_SUCCESS);

	CHECK_DO(proc_net_dev_parse_lines(rx2, tx2, devs)== num_devs, goto end);

	ret_array= (char**)calloc((num_devs* PROC_NET_DEV_PARAMS_PER_TAG)+ 1,
			sizeof(char*)); // '+1'-> 'NULL' terminated
	CHECK_DO(ret_array!= NULL, goto end);

	for(i= 0, j= 0; i< num_devs; i++, j+= PROC_NET_DEV_PARAMS_PER_TAG) {
		uint64_t rx_avg, tx_avg;

		CHECK_DO(rx2[i]>= rx1[i], goto end);
		// { // Comment-me: force fail to test fail-behavior
		//CHECK_DO(rx2[i]< rx1[i], goto end);
		// }
		rx_avg= (rx2[i]- rx1[i])/128; //128= 1024/8 [Bps-> Kbps]
		tx_avg= (tx2[i]- tx1[i])/128;
#if 0 // Comment-me
		LOG("%s avg[kbps]: Rx: %" PRIu64 ", Tx: %" PRIu64 "\n",
				devs[i], rx_avg, tx_avg);
#endif

		/* We will print 'PROC_NET_DEV_PARAMS_PER_TAG' number of parameters
		 * per network device/ tag */
		// Device name (1st parameter to print)
		ret_array[j]= strndup(devs[i], PROC_NET_DEV_SIZEOFTAG_MAX);
		CHECK_DO(ret_array[j]!= NULL, goto end);
		// Rx average (2nd parameter)
		ret_array[j+ 1]= (char*)malloc(NUM_CHAR_REPRESENTATION);
		CHECK_DO(ret_array[j+ 1]!= NULL, goto end);
		snprintf(ret_array[j+ 1], NUM_CHAR_REPRESENTATION, "%" PRIu64, rx_avg);
		// Tx average (3rd parameter)
		ret_array[j+ 2]= (char*)malloc(NUM_CHAR_REPRESENTATION);
		CHECK_DO(ret_array[j+ 2]!= NULL, goto end);
		snprintf(ret_array[j+ 2], NUM_CHAR_REPRESENTATION, "%" PRIu64, tx_avg);
	}

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		proc_net_dev_release(&ret_array);

	return ret_array;
}

void proc_net_dev_release(char ***ref_array)
{
	char **p;
	int i;

	if(ref_array== NULL)
		return;

	if((p= (char**)*ref_array)!= NULL) {
		for(i= 0; p[i]!= NULL; i++) {
			free(p[i]);
			p[i]= NULL;
		}
		free(p);
		*ref_array= NULL;
	}
}

/*
 * Example ($cat /proc/net/dev)
 * Inter-|Receive                                                |  Transmit
 *  face |bytes packets errs drop fifo frame compressed multicast|bytes ...
 *   eth0: ... <16 values>
 *     lo: ... <16 values>
 *  wlan0: ... <16 values>
 */
static int proc_net_dev_parse_lines(uint64_t rx[PROC_NET_DEV_MAX_DEV_NUM],
		uint64_t tx[PROC_NET_DEV_MAX_DEV_NUM],
		char devs[PROC_NET_DEV_MAX_DEV_NUM][PROC_NET_DEV_SIZEOFTAG_MAX])
{
	char line[LINE_SIZE_MAX];
	FILE *fp= NULL;
	int num_device= 0;

	fp= fopen(PROC_NET_DEV_FILE_PATH,"r");
	CHECK_DO(fp!= NULL, return STAT_ERROR);

	while(fgets(line, LINE_SIZE_MAX, fp)!= NULL &&
			num_device< PROC_NET_DEV_MAX_DEV_NUM) {
		/* Parse lines with network statistics */
		if(strstr(line, PROC_NET_DEV_TAG_1)!= NULL ||
		   strstr(line, PROC_NET_DEV_TAG_2)!= NULL ||
		   strstr(line, PROC_NET_DEV_TAG_3)!= NULL ||
		   strstr(line, PROC_NET_DEV_TAG_4)!= NULL ||
		   strstr(line, PROC_NET_DEV_TAG_5)!= NULL) {
			uint64_t v[16]= {0}; char str[PROC_NET_DEV_SIZEOFTAG_MAX]= {0};
			sscanf(line,"%s %" PRIu64 "%" PRIu64 "%" PRIu64 "%" PRIu64
					"%" PRIu64 "%" PRIu64 "%" PRIu64 "%" PRIu64
					"%" PRIu64 "%" PRIu64 "%" PRIu64 "%" PRIu64
					"%" PRIu64 "%" PRIu64 "%" PRIu64 "%" PRIu64,
					str, &v[0], &v[1], &v[2], &v[3],
					&v[4], &v[5], &v[6], &v[7],
					&v[8], &v[9], &v[10], &v[11],
					&v[12], &v[13], &v[14], &v[15]);
			rx[num_device]= v[0];
			tx[num_device]= v[8];
			str[strlen(str)- 1]=
					'\0'; // remove ':' character (e.g.: 'eth0:'-> 'eth0')
			strncpy(devs[num_device], str, PROC_NET_DEV_SIZEOFTAG_MAX);
#if 0 // Comment-me
			LOG("%s %" PRIu64 "; %" PRIu64 "\n",
					devs[num_device], rx[num_device], tx[num_device]);
#endif
			num_device++;
		}
	}
	fclose(fp);
	return num_device;
}
