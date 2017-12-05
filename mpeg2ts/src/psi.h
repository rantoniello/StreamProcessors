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
 * @file psi.h
 * @brief Program Specific Information (PSI) module.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_SRC_PSI_H_
#define SPMPEG2TS_SRC_PSI_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

#define PSI_SECTION_FIXED_LEN 12 // (8+ 1+ 1+ 2+ 12+ 18+ 5+ 1+ 8+ 8+ 32)/ 8
#define PSI_PAS_PROG_LEN 4

/**
 * Program-specific information classification table: PID numbers
 * (Rec. ITU-T H.222.0 (10/2014)).
 */
/**
 * Program Association Table (PAT)
 */
#define PSI_PAT_PID_NUMBER 	0x00
/**
 * Conditional Access Table (CAT)
 */
#define PSI_CAT_PID_NUMBER 	0x01
/**
 * Transport Stream Description Table
 */
#define PSI_DESCR_TABLE_PID_NUMBER 	0x02
/**
 * IPMP Control Information Table
 */
#define PSI_IPMP_TABLE_PID_NUMBER 0x03

/**
 * 'table_id' assignment values (Rec. ITU-T H.222.0 (10/2014))
 */
#define PSI_TABLE_PROGRAM_ASSOCIATION_SECTION 		0x00
#define PSI_TABLE_CONDITIONAL_ACCESS_SECTION 		0x01
#define PSI_TABLE_TS_PROGRAM_MAP_SECTION 			0x02
#define PSI_TABLE_TS_DESCRIPTION_SECTION 			0x03
#define PSI_TABLE_ISO_IEC_14496_SCENE_DESC_SECTION 	0x04
#define PSI_TABLE_ISO_IEC_14496_OBJECT_DESC_SECTION 0x05
#define PSI_TABLE_METADATA_SECTION 					0x06
#define PSI_TABLE_IPMP_CONTROL_INFORMATION_SECTION 	0x07
#define PSI_TABLE_ISO_IEC_14496_SECTION 			0x08
#define PSI_TABLE_RESERVED_MIN 						0x09
#define PSI_TABLE_RESERVED_MAX 						0x37
#define PSI_TABLE_DEFINED_ISO_IEC_13818_6_MIN 		0x38
#define PSI_TABLE_DEFINED_ISO_IEC_13818_6_MAX 		0x3F
#define PSI_TABLE_USER_PRIVATE_MIN 					0x40
#define PSI_TABLE_USER_PRIVATE_MAX 					0xFE
#define PSI_TABLE_FORBIDDEN 						0xFF

/**
 * Max. length value is 1024 bytes for Rec. ITU-T H.222.0 sections or 4096 for
 * private sections.
 */
#define PSI_TABLE_MAX_SECTION_LEN 4096
#define PSI_TABLE_MPEG_MAX_SECTION_LEN 1024

/* Forward declarations */
typedef struct psi_desc_ctx_s psi_desc_ctx_t;
typedef struct llist_s llist_t;

/**
 * Generic Program Specific Information section context structure
 */
typedef struct psi_section_ctx_s {
	/**
	 * (8 bits) Identifies to which table the section belongs. Refer to
	 * 'table_id' assignment values in the definitions above.
	 */
	uint8_t table_id;
	/**
	 * (1 bit) Should be set to '1' for Rec. ITU-T H.222.0 (10/2014) defined
	 * PSI sections. In the case of private PSI sections, when set to '1', it
	 * indicates that the private section follows the "generic section" syntax
	 * beyond the 'private_section_length' field. When set to '0', it indicates
	 * that the private_data_bytes immediately follow the
	 * 'private_section_length' field.
	 */
	uint8_t section_syntax_indicator;
	/**
	 * (1 bit) '0' for Rec. ITU-T H.222.0 (10/2014) defined PSI sections;
	 * 'private_indicator' for private PSI sections.
	 */
	uint8_t indicator_1;
	/**
	 * (2 bits)
	 */
	uint8_t  reserved_1;
	/**
	 * (12 bits) The first two bits should be '00'. The remaining 10 bits
	 * specify the number of bytes of the section starting immediately
	 * following the 'section_length' field, and including the 'CRC'.
	 */
	uint16_t section_length;

	/** IMPORTANT NOTE: The fields below do not mandatory apply for private
	 * sections (Rec. ITU-T H.222.0 (10/2014)). Only if
	 * 'section_syntax_indicator' is set to '1' the private section follows
	 * the "generic section" syntax below.
	 */

	/**
	 * (18 bits) This reserved field can be:
	 * - (16+ 2 bits) PAS: 'transport_stream_id'+ reserved
	 * - (18 bits)    CAS: 'reserved'
	 * - (16+ 2 bits) PMS: 'program_number'+ reserved
	 * - (18 bits)    TSDS: 'reserved'
	 * - privately defined for private sections
	 */
	uint16_t table_id_extension; // Rec. ITU-T H.222.0 (10/2014), Annex C.3.
	uint8_t reserved_2;

	/**
	 * (5 bits) When the characteristics of the transport stream described in
	 * the PSI change (e.g., extra programs added, different composition of
	 * elementary streams for a given program), then new PSI data has to be
	 * sent with the updated information as the most recently transmitted
	 * version of the sections marked as "current" must always be valid.
	 * Decoders need to be able to identify whether the most recently received
	 * section is identical with the section they have already processed/stored
	 * (in which case the section can be discarded), or whether it is
	 * different, and may therefore signify a configuration change.
	 */
	uint8_t  version_number;
	/**
	 * (1 bit) Signals at what point in the bitstream the PSI is valid. Each
	 * section can therefore be numbered as valid "now" (current), or as valid
	 * in the immediate future (next). This allows the transmission of a
	 * future configuration in advance of the change, giving the decoder the
	 * opportunity to prepare for the change. There is no obligation to
	 * transmit the next version of a section in advance, but if it is
	 * transmitted, then it shall be the next correct version of that section.
	 */
	uint8_t  current_next_indicator;
	/**
	 * (8 bits) The section_number field allows the sections of a particular
	 * table to be reassembled in their original order by the decoder. There
	 * is no obligation that sections must be transmitted in numerical order,
	 * but recommended by Rec. ITU-T H.222.0 (10/2014) */
	uint8_t  section_number;
	/**
	 * (8 bits) Specifies the number of the last section (that is, the section
	 * with the highest section_number) of a complete PSI table.
	 */
	uint8_t  last_section_number;
	/**
	 * Section data specific to:
	 * - program association section or
	 * - conditional access section or
	 * - program map section or ...
	 */
	void *data;
	/**
	 * (32 bits) Contains the CRC value that gives a zero output of the
	 * registers in the decoder defined in Annex A of Rec. ITU-T H.222.0
	 * (10/2014).
	 */
	uint32_t crc_32;

} psi_section_ctx_t;

/**
 * Program Association Section (PAS) specific data context structure
 */
typedef struct psi_pas_prog_ctx_s {
	/**
	 * (16 bits) Specifies the program to which the 'reference_pid' is
	 * applicable. When set to 0x0000, then 'reference_pid'== network PID.
	 * Else, the value of this field is user defined. This field shall not take
	 * any single value more than once within one version of the PAT.
	 */
	uint16_t program_number;
	//uint8_t  reserved; // (3 bits)
	/**
	 * (13 bits)
	 * if(program_number== '0')
	 *     reference_pid<-'network_PID'
	 * else
	 *     reference_pid<-'program_map_PID'
	 */
	uint16_t reference_pid;
} psi_pas_prog_ctx_t;

typedef struct psi_pas_ctx_s {
	/**
	 * List of pointers to PAS specific data context structures
	 * (type 'psi_pas_prog_ctx_t')
	 */
	llist_t *psi_pas_prog_ctx_llist;
} psi_pas_ctx_t;

/**
 * Elementary stream information context structure, carried as part of the
 * Program Map Section.
 */
typedef struct psi_pms_es_ctx_s {
	/**
	 * (8 bits) Specifies the type of program element carried within the
	 * packets with the PID whose value is specified by the elementary_PID.
	 */
	uint8_t stream_type;
	/**
	 * (3 bits)
	 */
	//uint8_t reserved_1;
	/**
	 * (13 bits) Specifies the PID of the transport stream packets which carry
	 * the associated program element.
	 */
	uint16_t elementary_PID;
	/**
	 * (4 bits)
	 */
	//uint8_t reserved_2;
	/**
	 * (12 bits) The first two bits shall be '00'. The remaining 10 bits
	 * specify the number of bytes of the descriptors of the associated
	 * program element immediately following the 'ES_info_length field'.
	 */
	uint16_t es_info_length;
	/**
	 * List of Elementary Stream (ES) information descriptors
	 * (type 'psi_desc_ctx_t').
	 */
	llist_t *psi_desc_ctx_llist;
} psi_pms_es_ctx_t;

/**
 * Program Map Section specific data context structure
 */
typedef struct psi_pms_ctx_s {
	/**
	 * (3 bits)
	 */
	//uint8_t reserved_1;
	/**
	 * (13 bits) PCR_PID specifies the PID of the transport stream packets
	 * which shall contain the PCR fields valid for the program specified by
	 * 'program_number'. If no PCR is associated with a program definition for
	 * private streams, then this field shall take the value of 0x1FFF.
	 */
	uint16_t pcr_pid;
	/**
	 * (4 bits)
	 */
	//uint8_t reserverd_2;
	/**
	 * (12 bits) The first two bits shall be '00'. The remaining 10 bits
	 * specify the number of bytes of the descriptors immediately following
	 * the 'program_info_length' field.
	 */
	uint16_t program_info_length;
	/**
	 * List of program information descriptors (type 'psi_desc_ctx_t').
	 */
	llist_t *psi_desc_ctx_llist;
	/**
	 * List of elementary stream information context structures
	 * (type 'psi_pms_es_ctx_t').
	 */
	llist_t *psi_pms_es_ctx_llist;
	/**
	 * (32 bits) CRC_32
	 */
	uint32_t crc_32;
} psi_pms_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
psi_section_ctx_t* psi_section_ctx_allocate();

/**
 * //TODO
 */
psi_section_ctx_t* psi_section_ctx_dup(
		const psi_section_ctx_t* psi_section_ctx);

/**
 * //TODO
 */
int psi_section_ctx_cmp(const psi_section_ctx_t* psi_section_ctx1,
		const psi_section_ctx_t* psi_section_ctx2);

/**
 * //TODO
 */
void psi_section_ctx_release(psi_section_ctx_t **ref_psi_section_ctx);

/**
 * //TODO
 */
void psi_section_ctx_trace(const psi_section_ctx_t *psi_section_ctx);

/**
 * //TODO
 */
psi_pas_ctx_t* psi_pas_ctx_allocate();

/**
 * //TODO
 */
psi_pas_ctx_t* psi_pas_ctx_dup(const psi_pas_ctx_t* psi_pas_ctx);

/**
 * //TODO
 */
void psi_pas_ctx_release(psi_pas_ctx_t **ref_psi_pas_ctx);

/**
 * //TODO
 */
void psi_pas_ctx_trace(const psi_pas_ctx_t *psi_pas_ctx);

/**
 * //TODO
 */
psi_pas_prog_ctx_t* psi_pas_prog_ctx_allocate();

/**
 * //TODO
 */
psi_pas_prog_ctx_t* psi_pas_prog_ctx_dup(
		const psi_pas_prog_ctx_t* psi_pas_prog_ctx);

/**
 * //TODO
 */
void psi_pas_prog_ctx_release(psi_pas_prog_ctx_t **ref_psi_pas_prog_ctx);

/**
 * //TODO
 */
void psi_pas_prog_ctx_trace(const psi_pas_prog_ctx_t *psi_pas_prog_ctx);

/**
 * //TODO
 */
psi_pms_ctx_t* psi_pms_ctx_allocate();

/**
 * //TODO
 */
psi_pms_ctx_t* psi_pms_ctx_dup(const psi_pms_ctx_t* psi_pms_ctx);

/**
 * //TODO
 */
psi_pms_es_ctx_t* psi_pms_ctx_filter_es_pid(const psi_pms_ctx_t *psi_pms_ctx,
		uint16_t es_pid);

/**
 * //TODO
 */
void psi_pms_ctx_release(psi_pms_ctx_t **ref_psi_pms_ctx);

/**
 * //TODO
 */
void psi_pms_ctx_trace(const psi_pms_ctx_t *psi_pms_ctx);

/**
 * //TODO
 */
psi_pms_es_ctx_t* psi_pms_es_ctx_allocate();

/**
 * //TODO
 */
psi_pms_es_ctx_t* psi_pms_es_ctx_dup(const psi_pms_es_ctx_t* psi_pms_es_ctx);

/**
 * //TODO
 */
void psi_pms_es_ctx_release(psi_pms_es_ctx_t **ref_psi_pms_es_ctx);

/**
 * //TODO
 */
void psi_pms_es_ctx_trace(const psi_pms_es_ctx_t *psi_pms_es_ctx);

#endif /* SPMPEG2TS_SRC_PSI_H_ */
