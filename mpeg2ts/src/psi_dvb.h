/*
 * Copyright (c) 2015 Rafael Antoniello
 *
 * This file is part of StreamProcessor.
 *
 * StreamProcessor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StreamProcessor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with StreamProcessor.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file psi_dvb.h
 * @brief Program Specific Information (PSI) specifications for SI (Service
 * Information) in DVB (Digital Video Broadcasting) systems.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_SRC_PSI_DVB_H_
#define SPMPEG2TS_SRC_PSI_DVB_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

/**
 * Coding of PID and table_id fields.
 * (ETSI EN 300 468 V1.14.1 (2014-05))
 */
#define PSI_DVB_NIT_PID_NUMBER 0x10
#define PSI_DVB_SDT_PID_NUMBER 0x11

/**
 * Allocation of table_id values.
 * (ETSI EN 300 468 V1.14.1 (2014-05))
 */
#define PSI_DVB_NETWORK_INFO_SECTION_ACTUAL  0x40
#define PSI_DVB_SERVICE_DESCR_SECTION_ACTUAL 0x42

/* Forward declarations */
typedef struct psi_desc_ctx_s psi_desc_ctx_t;
typedef struct llist_s llist_t;

/**
 * Single service description information.
 */
#define PSI_DVB_SDS_PROG_FIXED_LEN 5 // (16+ 6+ 1+ 1+ 3+ 1+ 12)/8 [bytes]
typedef struct psi_dvb_sds_prog_ctx_s {
	/**
	 * (16 bits) This is a 16-bit field which serves as a label to identify
	 * this service from any other service within the TS. The service_id is the
	 * same as the program_number in the corresponding program_map_section.
	 */
	uint16_t service_id;
	/**
	 * (6 bits) Reserved #1
	 */
	//uint8_t reserved_future_use_1;
	/**
	 * (1 bit)
	 */
	uint8_t EIT_schedule_flag;
	/**
	 * (1 bit)
	 */
	uint8_t EIT_present_following_flag;
	/**
	 * (3 bits)
	 */
	uint8_t running_status;
	/**
	 * (1 bit)
	 */
	uint8_t free_CA_mode;
	/**
	 * (12 bits) Gives the total length in bytes of the following descriptors.
	 */
	uint16_t descriptors_loop_length;
	/**
	 * DVB descriptors data list (type 'psi_desc_ctx_t').
	 */
	llist_t *psi_desc_ctx_llist;

} psi_dvb_sds_prog_ctx_t;

/**
 * Service Description Section (SDS) specific data context structure
 * (ETSI EN 300 468 V1.14.1 (2014-05))
 */
typedef struct psi_dvb_sds_ctx_s {
	/**
	 * (16 bits) Gives the label identifying the network_id of the originating
	 * delivery system.
	 */
	uint16_t original_network_id;
	/**
	 * (8 bits) Reserved #1
	 */
	//uint8_t reserved_future_use_1;
	/**
	 * Services data list (type 'psi_dvb_sds_prog_ctx_t').
	 */
	llist_t *psi_dvb_sds_prog_ctx_llist;
} psi_dvb_sds_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
psi_dvb_sds_ctx_t* psi_dvb_sds_ctx_allocate();

/**
 * //TODO
 */
psi_dvb_sds_ctx_t* psi_dvb_sds_ctx_dup(
		const psi_dvb_sds_ctx_t* psi_dvb_sds_ctx);

/**
 * //TODO
 */
void psi_dvb_sds_ctx_release(psi_dvb_sds_ctx_t **ref_psi_dvb_sds_ctx);

/**
 * Iterate over given SDS and get service specific data for a given program
 * number.
 * //TODO
 */
psi_dvb_sds_prog_ctx_t* psi_dvb_sds_ctx_filter_program_num(
		const psi_dvb_sds_ctx_t* psi_dvb_sds_ctx, uint16_t program_number);

/**
 * //TODO
 */
psi_dvb_sds_prog_ctx_t* psi_dvb_sds_prog_ctx_allocate();

/**
 * //TODO
 */
psi_dvb_sds_prog_ctx_t* psi_dvb_sds_prog_ctx_dup(
		const psi_dvb_sds_prog_ctx_t* psi_dvb_sds_prog_ctx);

/**
 * //TODO
 */
void psi_dvb_sds_prog_ctx_release(
		psi_dvb_sds_prog_ctx_t **ref_psi_dvb_sds_prog_ctx);

#endif /* SPMPEG2TS_SRC_PSI_DVB_H_ */
