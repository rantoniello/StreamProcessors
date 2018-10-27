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
 * @file proc_stat_cpu.h
 * @brief CPU load statistics module.
 * @Author Rafael Antoniello
 */

#ifndef STATS_SRC_PROC_STAT_CPU_H_
#define STATS_SRC_PROC_STAT_CPU_H_

/* **** Definitions **** */

#define PROC_STAT_NUM_CPU_MAX 32

typedef struct interr_usleep_ctx_s interr_usleep_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
int proc_stat_cpu_get_num_cpus();

/**
 * Get overall average CPU load value and individual CPU load values per
 * core-thread.
 * Values are read from well known system statistic file "/proc/stat".
 * //TODO
 * @return NULL terminated array of strings with values; e.g.:
 * {"average CPU load value", "CPU#1 load value", "CPU#2 load value", ...,
 * "CPU#N load value", NULL}
 */
char** proc_stat_cpu_get(interr_usleep_ctx_t *interr_usleep_ctx);

/**
 * Release array of strings previously obtained by calling function
 * 'proc_stat_cpu_get()'.
 * @param parray reference to array of strings.
 */
void proc_stat_cpu_release(char ***ref_array);

#endif /* STATS_SRC_PROC_STAT_CPU_H_ */
