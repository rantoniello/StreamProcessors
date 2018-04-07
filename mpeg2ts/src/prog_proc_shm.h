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
 * @file prog_proc_shm.h
 * @brief MPEG2-SPTS/MPTS program processor shared-memory commons.
 * @author Rafael Antoniello
 */

#ifndef STREAMPROCESSORS_MPEG2TS_SRC_PROG_PROC_SHM_H_
#define STREAMPROCESSORS_MPEG2TS_SRC_PROG_PROC_SHM_H_

/* **** Definitions **** */

#define ENABLE_DEBUG_LOGS
#ifdef ENABLE_DEBUG_LOGS
	#define LOGD_CTX_INIT(CTX) LOG_CTX_INIT(CTX)
	#define LOGD(FORMAT, ...) LOGV(FORMAT, ##__VA_ARGS__)
#else
	#define LOGD_CTX_INIT(CTX)
	#define LOGD(...)
#endif

/** Installation directory complete path */
#ifndef _INSTALL_DIR //HACK: "fake" path for IDE
#define _INSTALL_DIR "./"
#endif

#define SUF_SH_I "-fifo-iput"
#define SUF_SH_O "-fifo-oput"
#define SUF_SH_API_I "-api-iput"
#define SUF_SH_API_O "-api-oput"
#define SUF_SH_FLG_EXIT "-flag-exit"

#define PROG_PROC_SHM_FIFO_SIZE_IPUT 256
#define PROG_PROC_SHM_FIFO_SIZE_OPUT 256
#define PROG_PROC_SHM_SIZE_IO_CHUNK 256
#define PROG_PROC_SHM_SIZE_CHUNK_PUT 8192
#define PROG_PROC_SHM_SIZE_CHUNK_GET (1024* 512)

#endif /* STREAMPROCESSORS_MPEG2TS_SRC_PROG_PROC_SHM_H_ */
