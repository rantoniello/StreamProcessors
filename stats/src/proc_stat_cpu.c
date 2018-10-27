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
 * @file proc_stat_cpu.c
 * @author Rafael Antoniello
 */

#include "proc_stat_cpu.h"

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
#define PROC_STAT_FILE_PATH "/proc/stat"
#define PROC_STAT_CPU_TAG "cpu"
#define PROC_STAT_MEASURE_PERIOD_USECS 1000*1000
#define NUM_CHAR_REPRESENTATION 5

/* **** Prototypes **** */

static int proc_stat_cpu_parse_lines(uint64_t val1[PROC_STAT_NUM_CPU_MAX],
		uint64_t val2[PROC_STAT_NUM_CPU_MAX]);

/* **** Implementations **** */

int proc_stat_cpu_get_num_cpus()
{
	char line[LINE_SIZE_MAX];
	FILE *fp= NULL;
	int num_cpus= 0;

	fp= fopen(PROC_STAT_FILE_PATH,"r");
	CHECK_DO(fp!= NULL, return STAT_ERROR);

	while(fgets(line, LINE_SIZE_MAX, fp)!= NULL &&
			num_cpus< PROC_STAT_NUM_CPU_MAX) {
		/* Parse lines with CPU statistics */
		if(strncmp(line, PROC_STAT_CPU_TAG, strlen(PROC_STAT_CPU_TAG))== 0)
			num_cpus++;
	}
	fclose(fp);
	return num_cpus;
}

char** proc_stat_cpu_get(interr_usleep_ctx_t *interr_usleep_ctx)
{
	int i, num_cpus= 0, ret_code, end_code= STAT_ERROR;
	char **ret_array= NULL;
	uint64_t a1[PROC_STAT_NUM_CPU_MAX]= {0}, a2[PROC_STAT_NUM_CPU_MAX]= {0},
	b1[PROC_STAT_NUM_CPU_MAX]= {0}, b2[PROC_STAT_NUM_CPU_MAX]= {0};

	/* Check arguments */
	CHECK_DO(interr_usleep_ctx!= NULL, goto end);

	num_cpus= proc_stat_cpu_parse_lines(a1, a2);
	CHECK_DO(num_cpus> 0, goto end);

	ret_code= interr_usleep(interr_usleep_ctx, PROC_STAT_MEASURE_PERIOD_USECS);
	if(ret_code== STAT_EINTR)
		goto end;
	ASSERT(ret_code== STAT_SUCCESS);

	CHECK_DO(proc_stat_cpu_parse_lines(b1, b2)== num_cpus, goto end);

	ret_array= (char**)calloc(num_cpus+ 1,
			sizeof(char*)); // '+1'-> 'NULL' terminated
	CHECK_DO(ret_array!= NULL, goto end);

	for(i= 0; i< num_cpus; i++) {
		uint64_t loadavg;

		CHECK_DO(b1[i]>= a1[i], goto end);
		CHECK_DO(b2[i]> a2[i], goto end);
		// { // Comment-me: force fail to test fail-behavior
		//CHECK_DO(b1[i]< a1[i], goto end);
		// }
		loadavg= ((b1[i]- a1[i])*100)/ (b2[i]- a2[i]);
#if 0 // Comment-me
		if(i== 0) {
			LOG("CPU average: %" PRIu64 "\n", loadavg);
		} else {
			LOG("CPU[%d]: %" PRIu64 "\n",i, loadavg);
		}
#endif
		ret_array[i]= (char*)malloc(NUM_CHAR_REPRESENTATION);
		CHECK_DO(ret_array[i]!= NULL, goto end);
		snprintf(ret_array[i], NUM_CHAR_REPRESENTATION, "%" PRIu64, loadavg);
	}

	end_code= STAT_SUCCESS;
end:
	if(end_code!= STAT_SUCCESS)
		proc_stat_cpu_release(&ret_array);

	return ret_array;
}

void proc_stat_cpu_release(char ***ref_array)
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
 *     user     nice  system  idle     iowait irq  softirq steal guest guest_nice
 * cpu 10035931 20584 1708969 54195343 546595 31   10418   0     0     0
 *     v[0]     v[1]  v[2]    v[3]     v[4]
 *
 * PrevIdle=previdle+previowait
 * Idle=idle+iowait
 * PrevNonIdle=prevuser+prevnice+prevsystem+previrq+prevsoftirq+prevsteal
 * NonIdle=user+nice+system+irq+softirq+steal
 * PrevTotal=PrevIdle+PrevNonIdle
 * Total=Idle+NonIdle
 * CPU_Percentage=((Total-PrevTotal)-(Idle-PrevIdle))/(Total-PrevTotal)
 */
static int proc_stat_cpu_parse_lines(uint64_t val1[PROC_STAT_NUM_CPU_MAX],
		uint64_t val2[PROC_STAT_NUM_CPU_MAX])
{
	char line[LINE_SIZE_MAX];
	FILE *fp= NULL;
	int num_cpus= 0;

	fp= fopen(PROC_STAT_FILE_PATH,"r");
	CHECK_DO(fp!= NULL, return STAT_ERROR);

	while(fgets(line, LINE_SIZE_MAX, fp)!= NULL &&
			num_cpus< PROC_STAT_NUM_CPU_MAX) {
		/* Parse lines with CPU statistics */
		if(strncmp(line, PROC_STAT_CPU_TAG, strlen(PROC_STAT_CPU_TAG))== 0) {
			uint64_t v[7]; char str[16];
			sscanf(line,"%s %" PRIu64 "%" PRIu64 "%" PRIu64 "%" PRIu64
					"%" PRIu64, str, &v[0],&v[1],&v[2],&v[3],&v[4]);
			val1[num_cpus]= v[0]+v[1]+v[2];
			val2[num_cpus]= v[0]+v[1]+v[2]+v[3]+v[4];
#if 0 // Comment-me
			LOG("%s %" PRIu64 "; %" PRIu64 "\n",
					line, val1[num_cpus], val2[num_cpus]);
			LOG("%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
					v[0], v[1], v[2], v[3], v[4]);
#endif
			num_cpus++;
		}
	}
	fclose(fp);
	return num_cpus;
}
