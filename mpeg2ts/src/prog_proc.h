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
 * @file prog_proc.h
 * @brief MPEG2-SPTS/MPTS program processor.
 * @author Rafael Antoniello
 */

#ifndef STREAMPROCESSORS_MPEG2TS_SRC_PROG_PROC_H_
#define STREAMPROCESSORS_MPEG2TS_SRC_PROG_PROC_H_

/* **** Definitions **** */

/* Forward definitions */
typedef struct proc_if_s proc_if_t;

/* **** prototypes **** */

/**
 * Processor interface implementing the program processor.
 */
extern const proc_if_t proc_if_mpeg2_prog_proc;

#endif /* STREAMPROCESSORS_MPEG2TS_SRC_PROG_PROC_H_ */
