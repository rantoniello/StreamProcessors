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
 * @file psi_table_proc.h
 * @brief Program Specific Information (PSI) processor.
 * @author Rafael Antoniello
 */

#ifndef STREAMPROCESSORS_MPEG2TS_SRC_PSI_TABLE_PROC_H_
#define STREAMPROCESSORS_MPEG2TS_SRC_PSI_TABLE_PROC_H_

/* **** Definitions **** */

/* Forward definitions */
typedef struct proc_if_s proc_if_t;

/* **** prototypes **** */

/**
 * Processor interface implementing the
 * MPEG2-TS Program Specific Information (PSI) table decoder/parser.
 */
extern const proc_if_t proc_if_psi_table_proc;

#endif /* STREAMPROCESSORS_MPEG2TS_SRC_PSI_TABLE_PROC_H_ */
