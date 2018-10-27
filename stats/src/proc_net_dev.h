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
 * @file proc_net_dev.h
 * @brief Network traffic statistics module.
 * @Author Rafael Antoniello
 */

#ifndef STATS_SRC_PROC_NET_DEV_H_
#define STATS_SRC_PROC_NET_DEV_H_

/* **** Definitions **** */

/** Maximum number of connected network devices that will be scanned for
 * statistical information */
#define PROC_NET_DEV_MAX_DEV_NUM 8

/** Number of parameters to scan per network device; these are the following:
 * device name, average received bit-rate, average transmitted bit-rate. */
#define PROC_NET_DEV_PARAMS_PER_TAG 3

typedef struct interr_usleep_ctx_s interr_usleep_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
int proc_net_dev_get_num_devs();

/**
 * Get received and transmitted average traffic, in kbps units, correspondent
 * to each individual network input/output device.
 * //TODO
 * Values are read from well known system statistic file "/proc/net/dev".
 * @return NULL terminated array of strings with the following format:
 * {"device#1 name", "dev#1 Rx average value", "dev#1 Tx average value", ...,
 *  "device#N name", "dev#N Rx average value", "dev#N Tx average value, NULL}
 */
char** proc_net_dev_get(interr_usleep_ctx_t *interr_usleep_ctx);

/**
 * Release array of strings previously obtained by calling function
 * 'proc_net_dev_get()'.
 * @param parray reference to array of strings.
 */
void proc_net_dev_release(char ***ref_array);

#endif /* STATS_SRC_PROC_NET_DEV_H_ */
