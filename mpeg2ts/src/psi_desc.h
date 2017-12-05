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
 * @file psi_desc.h
 * @brief Program Specific Information (PSI) descriptors.
 * @Author Rafael Antoniello
 */

#ifndef SPMPEG2TS_SRC_PSI_DESC_H_
#define SPMPEG2TS_SRC_PSI_DESC_H_

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

/**
 * Descriptors tags.
 */
#define PSI_DESC_TAG_DVB_SERVICE 0x48
#define PSI_DESC_TAG_DVB_SUBT 0x59

/* Forward declarations */
typedef struct llist_s llist_t;
typedef struct log_ctx_s log_ctx_t;
typedef struct cJSON cJSON;

typedef struct psi_desc_lu_ctx_s {
	uint8_t descriptor_tag;
	const char *name;
} psi_desc_lu_ctx_t;

extern const psi_desc_lu_ctx_t psi_desc_lutable[256];

/**
 * Generic descriptor context structure.
 * Program descriptors are structures which may be used to extend the
 * definitions of programs and program elements. All descriptors have a format
 * which begins with an 8-bit tag value. The tag value is followed by an 8-bit
 * descriptor length and data fields.
 */
#define PSI_DESC_FIXED_LEN 2 // (8+ 8)/8 [bytes]
typedef struct psi_desc_ctx_s {
	/**
	 * (8 bits) Field which identifies each descriptor
	 */
	uint8_t descriptor_tag;
	/**
	 * (8 bits) Number of bytes of the descriptor immediately following
	 * descriptor_length field.
	 */
	uint8_t descriptor_length;
	/**
	 * Descriptor data fields.
	 */
	void *data;
} psi_desc_ctx_t;

/**
 * DVB Service descriptor (Tag 0x48).
 */
typedef struct psi_desc_dvb_service_ctx_s {
	/**
	 * (8 bits)
	 */
	uint8_t service_type;
	/**
	 * (8 bits) Specifies the number of bytes that follow the
	 * 'service_provider_name_length' field for describing characters of the
	 * name of the service provider.
	 */
	uint8_t service_provider_name_length;
	/**
	 * Service provider name.
	 */
	char service_provider_name[256];
	/**
	 * (8 bits) Specifies the number of bytes that follow the
	 * 'service_name_length' field for describing characters of the name of
	 * the service.
	 */
	uint8_t service_name_length;
	/**
	 * Service name.
	 */
	char service_name[256];

} psi_desc_dvb_service_ctx_t;

/**
 * DVB Subtitling descriptor (Tag 0x59).
 */
typedef struct psi_desc_dvb_subt_nth_ctx_s {
	/**
	 * (24 bits) This 24-bit field contains the ISO 639-2 three character
	 * language code of the language of the subtitle.
	 */
	uint32_t _639_language_code;
	/**
	 * (8 bits)  This 8 bit field provides information on the content of the
	 * subtitle and the intended display.
	 */
	uint8_t subtitling_type;
	/**
	 * (16 bits)
	 */
	uint16_t composition_page_id;
	/**
	 * (16 bits)
	 */
	uint16_t ancillary_page_id;
} psi_desc_dvb_subt_nth_ctx_t;

typedef struct psi_desc_dvb_subt_ctx_s {
	/**
	 * List of subtitling descriptor data.
	 */
	llist_t *psi_desc_dvb_subt_nth_ctx_llist;
} psi_desc_dvb_subt_ctx_t;

/* **** Prototypes **** */

/**
 * //TODO
 */
psi_desc_ctx_t* psi_desc_ctx_allocate();

/**
 * //TODO
 */
psi_desc_ctx_t* psi_desc_ctx_dup(const psi_desc_ctx_t* psi_desc_ctx);

/**
 * //TODO
 */
void psi_desc_ctx_release(psi_desc_ctx_t **ref_psi_desc_ctx);

/**
 * //TODO
 */
psi_desc_ctx_t* psi_desc_ctx_filter_tag(llist_t *psi_desc_ctx_llist,
		int *ret_index, int tag);

/**
 * //TODO
 */
psi_desc_dvb_service_ctx_t* psi_desc_dvb_service_ctx_allocate();

/**
 * //TODO
 */
psi_desc_dvb_service_ctx_t* psi_desc_dvb_service_ctx_dup(
		const psi_desc_dvb_service_ctx_t* psi_desc_dvb_service_ctx);

/**
 * //TODO
 */
void psi_desc_dvb_service_ctx_release(
		psi_desc_dvb_service_ctx_t **ref_psi_desc_dvb_service_ctx);

/**
 * //TODO
 */
psi_desc_dvb_subt_ctx_t* psi_desc_dvb_subt_ctx_allocate();

/**
 * //TODO
 */
psi_desc_dvb_subt_ctx_t* psi_desc_dvb_subt_ctx_dup(
		const psi_desc_dvb_subt_ctx_t* psi_desc_dvb_subt_ctx);

/**
 * //TODO
 */
void psi_desc_dvb_subt_ctx_release(
		psi_desc_dvb_subt_ctx_t **ref_psi_desc_dvb_subt_ctx);

/**
 * //TODO
 */
int psi_desc_dvb_subt_rest_get(const psi_desc_ctx_t *psi_desc_ctx_subt,
		cJSON **ref_cjson_subt_descript, log_ctx_t *log_ctx);

/**
 * //TODO
 */
int psi_desc_dvb_subt_rest_put(const cJSON *cjson_subt_descript,
		psi_desc_ctx_t **ref_psi_desc_ctx_subt, log_ctx_t *log_ctx);

/**
 * //TODO
 */
psi_desc_dvb_subt_nth_ctx_t* psi_desc_dvb_subt_nth_ctx_allocate();

/**
 * //TODO
 */
psi_desc_dvb_subt_nth_ctx_t* psi_desc_dvb_subt_nth_ctx_dup(
		const psi_desc_dvb_subt_nth_ctx_t* psi_desc_dvb_subt_nth_ctx);

/**
 * //TODO
 */
void psi_desc_dvb_subt_nth_ctx_release(
		psi_desc_dvb_subt_nth_ctx_t **ref_psi_desc_dvb_subt_nth_ctx);

/**
 * //TODO
 */
int psi_desc_dvb_subt_nth_rest_get(
		const psi_desc_dvb_subt_nth_ctx_t* psi_desc_dvb_subt_nth_ctx,
		cJSON **ref_cjson_descript_service, log_ctx_t *log_ctx);

/**
 * //TODO
 */
int psi_desc_dvb_subt_nth_rest_put(const cJSON *cjson_descript_service,
		psi_desc_dvb_subt_nth_ctx_t **ref_psi_desc_dvb_subt_nth_ctx,
		log_ctx_t *log_ctx);

#endif /* SPMPEG2TS_SRC_PSI_DESC_H_ */
