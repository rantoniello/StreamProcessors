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
 * @file mpeg2_sp.h
 * @brief MPEG2 stream processor module.<br>
 * This processor performs the following loopback:<br>
 * - Receives and demultiplex MPEG2-SPTS/MPTS;
 * - Data is organized in programs and sent to 'program processors'. These
 * processors are instantiated and released by the application user via the
 * API;
 * - Each program processor demultiplex the data in elementary streams and
 * provides a set of configurable transcoders for each one. Transcoded
 * (or by-passed) elementary streams are interleaved again to compose the
 * output processed program;
 * - Each processed program can be streamed out in MPEG2-SPTS or other
 * available output protocol.
 *
 * @author Rafael Antoniello
 */

#ifndef STREAMPROCESSORS_MPEG2TS_SRC_MPEG2_SP_H_
#define STREAMPROCESSORS_MPEG2TS_SRC_MPEG2_SP_H_

/* **** Definitions **** */

/* Forward definitions */
typedef struct proc_if_s proc_if_t;

/* **** prototypes **** */

/**
 * Processor interface implementing the MPEG2-SPTS/MPTS stream processor.
 */
extern const proc_if_t proc_if_mpeg2_sp;

#endif /* STREAMPROCESSORS_MPEG2TS_SRC_MPEG2_SP_H_ */
