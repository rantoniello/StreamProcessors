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
 * @file stats.h
 * @brief System statistics module.
 * @author Rafael Antoniello
 */

#ifndef STATS_SRC_STATS_H_
#define STATS_SRC_STATS_H_

/* **** Definitions **** */

#define STATS_WINDOW_SECS 60

typedef enum stats_id_enum {
	STATS_CPU= 0,
	STATS_NETWORK,
	STATS_RSS,
	STATS_ID_MAX
} stats_id_t;

/* Forward declarations */
typedef struct stats_ctx_s stats_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 * This is an example of function prototype documentation (Doxygen’s style)
 * @param arg1 …
 * @return …
 */
int stats_module_open();

/**
 * //TODO
 * This is an example of function prototype documentation (Doxygen’s style)
 * @param arg1 …
 * @return …
 */
void stats_module_close();

/**
 * //TODO
 * This is an example of function prototype documentation (Doxygen’s style)
 * @param arg1 …
 * @return …
 */
char* stats_module_get(stats_id_t id);

#endif /* STATS_SRC_STATS_H_ */
