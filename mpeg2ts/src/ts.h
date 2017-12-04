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
 * @file ts.h
 * @brief Transport stream layer module.
 * @author Rafael Antoniello
 */

#ifndef STREAMPROCESSORS_MPEG2TS_SRC_TS_H_
#define STREAMPROCESSORS_MPEG2TS_SRC_TS_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

/**
 * ISO/IEC 13818-1 Transport Stream packets are 188 bytes in length.
 */
#define TS_PKT_SIZE 188
#define TS_PKTS_PER_UDP 7 // "legacy UDP"
#define TS_PKT_PREFIX_LEN 4
#define TS_TIMESTAMP_INVALID (-1)
#define TS_CC_UNDEF 0xFF

/**
 * Maximum valid value for the Packet IDentifier (PID).
 */
#define TS_MAX_PID_VAL 0x1FFF

/**
 * Get PID value from binary MPEG2-TS packet.
 */
#define TS_BUF_GET_PID(TS_PKT_POINTER) \
	(uint16_t)\
	((\
     ((uint16_t)(((uint8_t*)(TS_PKT_POINTER))[1])<< 8) |\
	 (((uint8_t*)(TS_PKT_POINTER))[2])\
    )& 0x1FFF)

/**
 * Get 'unit start indicator' field value from binary MPEG2-TS packet.
 */
#define TS_BUF_GET_START_INDICATOR(TS_PKT_POINTER) \
	(uint8_t)((((uint8_t*)(TS_PKT_POINTER))[1])& 0x40)

/**
 * Get 'payload flag' field value from binary MPEG2-TS packet.
 */
#define TS_BUF_GET_PAYLOAD_FLAG(TS_PKT_POINTER) \
	(uint8_t)((((uint8_t*)(TS_PKT_POINTER))[3])& 0x10)

/**
 * Get 'continuity counter' field value from binary MPEG2-TS packet.
 */
#define TS_BUF_GET_CC(TS_PKT_POINTER) \
	(uint8_t)((((uint8_t*)(TS_PKT_POINTER))[3])& 0x0F)

/** MPEG-2 Transport Stream (TS) Adaptation Field (AF) context structure */
typedef struct ts_af_ctx_s {
	/**
	 * (8 bits) Number of bytes in the adaptation field immediately following
	 * this byte.
	 * The value '0' is for inserting a single stuffing byte in the adaptation
	 * field of a transport stream packet.
	 */
	uint8_t  adaptation_field_length;
	/**
	 * (1 bit) '1' if current TS packet is in a discontinuity state with CC/PCR
	 */
	uint8_t  discontinuity_indicator;
	/**
	 * (1 bit) '1' if the PES packet in this TS packet starts a video/audio
	 * sequence
	 */
	uint8_t  random_access_indicator;
	/**
	 * (1 bit) '1'= higher priority
	 */
	uint8_t  elementary_stream_priority_indicator;
	/**
	 * (1 bit) '1' if adaptation field contains a PCR field
	 */
	uint8_t  pcr_flag;
	/**
	 * (1 bit) '1' if adaptation field contains an OPCR field
	 */
	uint8_t  opcr_flag;
	/**
	 * (1 bit) '1' if adaptation field contains a splice count-down field
	 */
	uint8_t  splicing_point_flag;
	/**
	 * (1 bit) '1' if adaptation field contains private data bytes
	 */
	uint8_t  transport_private_data_flag;
	/**
	 * (1 bit) '1' if adaptation field contains an extension
	 */
	uint8_t  adaptation_field_extension_flag;

	/**
	 * Below fields are optional variables, depending on flags
	 */
	/**
	 * (33+6+9 bits) Program clock reference, stored in 6 octets in big-endian
	 * as 33 bits base, 6 bits reserved, 9 bits extension.
	 * Equations:
	 * PCR(i) = PCR_base(i) × 300 + PCR_ext(i); where:
	 * PCR_base(i) = ((system_clock_frequency × t(i)) DIV 300)% 2^33;
	 * PCR_ext(i)= ((system_clock_frequency × t(i)) DIV 1)% 300;
	 * system_clock_frequency is normally 27MHz;
	 * DIV is an integer division with truncation of the result towards
	 * negative infinite.
	 * Reserved bits shall be set to '1'.
	 */
	uint64_t pcr;
	/**
	 * (33+6+9 bits) Original Program clock reference. Helps when one TS is
	 * copied into another.
	 */
	uint64_t opcr;
	/**
	 * (8 bits) Indicates how many TS packets from this one a splicing point
	 * occurs (may be negative)
	 */
	uint8_t  splice_countdown;
	/**
	 * Rest of AF remaining bytes allocation pointer
	 */
	uint8_t *af_remaining;
	/**
	 * Rest of AF remaining bytes allocation size
	 */
	uint8_t af_remaining_size; // max. value is '188- 4- 1'
} ts_af_ctx_t;

/** MPEG-2 Transport Stream (TS) context structure */
typedef struct ts_ctx_s {
	/**
	 * (1 bit) TEI. Inform a stream processor to ignore the packet
	 */
	uint8_t transport_error_indicator;
	/**
	 * (1 bit) 'true' meaning the start of PES/PSI data; otherwise zero only.
	 */
	uint8_t payload_unit_start_indicator;
	/**
	 * (1 bit) 'true' meaning the current packet has a higher priority
	 */
	uint8_t transport_priority;
	/**
	 * (13 bits) Packet Identifier
	 */
	uint16_t pid;
	/**
	 * (2 bits) '00'= Not scrambled/'01'= Reserved/'10' '11'= Scrambled with
	 * even/odd key
	 */
	uint8_t scrambling_control;
	/**
	 * (1 bit) Boolean flag
	 */
	uint8_t adaptation_field_exist;
	/**
	 * (1 bit) Boolean flag
	 */
	uint8_t contains_payload;
	/**
	 * (4 bits) Sequence number of payload packets (0x00 to 0x0F).
	 */
	uint8_t continuity_counter;
	/**
	 * Adaptation Field (AF) context structure pointer
	 */
	ts_af_ctx_t *adaptation_field;
	/**
	 * Payload allocation pointer
	 */
	uint8_t *payload;
	/**
	 * Payload allocation size in bytes
	 */
	uint8_t payload_size; // max. value is '188- 4'
} ts_ctx_t;

/* **** Prototypes **** */

/**
 * Allocate an MPEG2-TS context structure. Structure is initialized to '0s'.
 * @return Newly allocated MPEG2-TS context structure.
 */
ts_ctx_t* ts_ctx_allocate();

/**
 * Make a copy of the given MPEG2-TS context structure.
 * @param ts_ctx Pointer to the MPEG2-TS context structure to copy.
 * @return New MPEG2-TS context structure replica.
 */
ts_ctx_t* ts_ctx_dup(const ts_ctx_t* ts_ctx);

/**
 * Trace MPEG2-TS context structure.
 * @param ts_ctx Pointer to the MPEG2-TS context structure to trace.
 */
void ts_ctx_trace(const ts_ctx_t *ts_ctx);

/**
 * Release MPEG2-TS context structure.
 * @param ref_ts_ctx Reference to the pointer to the MPEG2-TS context
 * structure to release.
 */
void ts_ctx_release(ts_ctx_t **ref_ts_ctx);

/**
 * Allocate a MPEG2-TS Adaptation Field context structure.
 * Structure is initialized to '0s'.
 * @return Newly allocated MPEG2-TS Adaptation Field context structure.
 */
ts_af_ctx_t* ts_af_ctx_allocate();

/**
 * Make a copy of the given MPEG2-TS Adaptation Field context structure.
 * @param ts_af_ctx Pointer to the MPEG2-TS Adaptation Field context structure
 * to copy.
 * @return New MPEG2-TS Adaptation Field context structure replica.
 */
ts_af_ctx_t* ts_af_ctx_dup(const ts_af_ctx_t* ts_af_ctx);

/**
 * Release MPEG2-TS Adaptation Field context structure.
 * @param ref_ts_af_ctx Reference to the pointer to the MPEG2-TS Adaptation
 * Field context structure to release.
 */
void ts_af_ctx_release(ts_af_ctx_t **ref_ts_af_ctx);

/**
 * Get MPEG2-TS header size (including the adaptation field if present).
 * @param ts_ctx Pointer to the MPEG2-TS context structure to parse.
 * @param hdr_size Pointer to an unsigned 8-bit integer in which the computed
 * header size value will be returned.
 * @return Status code (refer to 'stat_codes_ctx_t' type).
 */
int ts_get_header_size(const ts_ctx_t* ts_ctx, uint8_t *hdr_size);

#endif /* STREAMPROCESSORS_MPEG2TS_SRC_TS_H_ */
