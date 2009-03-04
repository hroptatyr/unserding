/*** catalogue.c -- unserding catalogue
 *
 * Copyright (C) 2008 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include "catalogue.h"
#include "protocore.h"
/* other external stuff */
#include <pfack/instruments.h>

#if defined UNSERSRV
typedef struct ud_cat_s *__cat_t;

static const char empty_msg[] = "empty\n";

/* the global catalogue */
static ud_cat_t ud_catalogue = NULL;
static size_t ud_catalen = 0;


/* helpers */
static inline signed char __attribute__((always_inline, gnu_inline))
tlv_cmp_f(const ud_tlv_t t1, const ud_tlv_t t2)
{
/* returns -1 if t1 < t2, 0 if t1 == t2 and 1 if t1 > t2 */
	uint8_t t1s = ud_tlv_size(t1);
	uint8_t t2s = ud_tlv_size(t2);
	uint8_t sz = t1s < t2s ? t1s : t2s;
	return memcmp((const char*)t1, (const char*)t2, sz);
}

void
__ud_fill_catobj(ud_catobj_t co, ...)
{
	va_list args;

        /* prepare list for va_arg */
        va_start(args, co);
	/* traverse the varargs list */
        for (uint8_t i = 0; i < co->nattrs; ++i ) {
		co->attrs[i] = va_arg(args, ud_tlv_t);
	}
	va_end(args);
	/* sort the args, is a selection sort now, will be heap sort one day */
	for (uint8_t j = 0; j < co->nattrs-1; j++) {
		uint8_t imin = j;
		ud_tlv_t tmin = co->attrs[imin];

		/* traverse the rest of the bugger, try and find the
		 * minimum element there */
		for (uint8_t i = imin; i < co->nattrs; ++i) {
			ud_tlv_t t = co->attrs[i];
			if (tlv_cmp_f(tmin, t) > 0) {
				tmin = t, imin = i;
			}
		}
		/* tmin @ imin is minimum element, swap co->attrs[j]
		 * and co->attrs[imin] */
		if (imin != j) {
			ud_tlv_t tmp = co->attrs[j];
			co->attrs[j] = co->attrs[imin];
			co->attrs[imin] = tmp;
		}
	}
	return;
}

void
ud_cat_add_obj(ud_catobj_t co)
{
	ud_cat_t new = malloc(sizeof(struct ud_cat_s));
	new->next = ud_catalogue;
	new->data = co;
	ud_catalen++;
	ud_catalogue = new;
	return;
}

static unsigned int
serialise_keyval(char *restrict buf, ud_tlv_t keyval)
{
	unsigned int idx = 2;
	/* buf[0] is KEYVAL type designator */
	buf[0] = UDPC_TYPE_KEYVAL;
	/* refactor me, lookup table? */
	switch ((ud_tag_t)(buf[1] = keyval->tag)) {
	case UD_TAG_CLASS:
		/* should be serialise_class */
		memcpy(&buf[idx], keyval->data, 1+keyval->data[0]);
		idx += keyval->data[0] + 1;
		break;
	case UD_TAG_NAME:
		/* should be serialise_name */
		memcpy(&buf[idx], keyval->data, 1+keyval->data[0]);
		idx += keyval->data[0] + 1;
		break;
	default:
		break;
	}
	return idx;
}

static unsigned int
serialise_catobj(char *restrict buf, ud_catobj_t co)
{
	unsigned int idx = 2;

	/* we are a UDPC_TYPE_CATOBJ */
	buf[0] = (udpc_type_t)UDPC_TYPE_CATOBJ;
	buf[1] = co->nattrs;
	for (uint8_t i = 0; i < co->nattrs; ++i) {
		idx += serialise_keyval(&buf[idx], co->attrs[i]);
	}
	return idx;
}

static unsigned int
sort_params(ud_tlv_t *tlvs, char *restrict wrkspc, job_t j)
{
	uint8_t idx = 0;
	uint8_t sz;
	ud_tlv_t tmin, last;

	if (UNLIKELY(j->buf[8] != UDPC_TYPE_SEQOF)) {
		return 0;
	} else if (UNLIKELY((idx = j->buf[9]) == 0)) {
		return 0;
	} else if (idx == 1) {
		ud_tlv_t tlv = (ud_tlv_t)&j->buf[10];
		memcpy(wrkspc, tlv, 2+ud_tlv_size(tlv));
		tlvs[0] = (ud_tlv_t)wrkspc;
		return 1;
	}

	/* do a primitive slection sort, do me properly! */
	/* we traverse the list once to find the minimum element */
	tmin = (ud_tlv_t)&j->buf[10];

	for (ud_tlv_t t = (ud_tlv_t)
		     ((char*)tmin + 2 + ud_tlv_size(tmin));
	     (char*)t < j->buf + j->blen;
	     t = (ud_tlv_t)((char*)t + 2 + ud_tlv_size(t))) {

		if (tlv_cmp_f(tmin, t) > 0) {
			tmin = t;
		}
	}
	/* copy the stuff over to the work space */
	tlvs[0] = (ud_tlv_t)wrkspc;
	memcpy(wrkspc, tmin, sz = 2 + ud_tlv_size(tmin));
	wrkspc += sz;

	/* using this as new maximum */
	last = tmin;

	for (uint8_t k = 1; k < idx; k++) {
		/* just choose a tmin */
		tmin = (ud_tlv_t)&j->buf[10];

		for (ud_tlv_t t = (ud_tlv_t)
			     ((char*)tmin + 2 + ud_tlv_size(tmin));
		     (char*)t < j->buf + j->blen;
		     t = (ud_tlv_t)((char*)t + 2 + ud_tlv_size(t))) {

			if (tlv_cmp_f(tmin, last) <= 0) {
				tmin = t;
				continue;
			}
			if (tlv_cmp_f(t, last) <= 0) {
				continue;
			}
			if (tlv_cmp_f(tmin, t) > 0) {
				tmin = t;
			}
		}
		tlvs[k] = (ud_tlv_t)wrkspc;
		memcpy(wrkspc, tmin, sz = 2 + ud_tlv_size(tmin));
		wrkspc += sz;

		/* using this as new maximum */
		last = tmin;
	}
	return idx;
}

static inline bool
catobj_filter(ud_catobj_t dat, ud_tlv_t *sub, uint8_t slen)
{
	uint8_t j = 0;
	for (uint8_t i = 0; j < slen && i < dat->nattrs; ) {
		switch (tlv_cmp_f(dat->attrs[i], sub[j])) {
		case 0:
			i++, j++;
			break;
		case 1:
			return false;
		case -1:
			i++;
			break;
		default:
			break;
		}
	}
	return j == slen;
}

/* some jobs to browse the catalogue */
extern bool ud_cat_ls_job(job_t j);
bool
ud_cat_ls_job(job_t j)
{
	unsigned int idx = 10;
	unsigned int slen = 0;
	char tmp[UDPC_SIMPLE_PKTLEN];
	ud_tlv_t sub[8];

	UD_DEBUG_CAT("ls job\n");
	/* filter what the luser sent us */
	slen = sort_params(sub, tmp, j);
	/* we are a seqof(UDPC_TYPE_CATOBJ) */
	j->buf[8] = (udpc_type_t)UDPC_TYPE_SEQOF;
	/* we are ud_catalen entries wide */
	j->buf[9] = (uint8_t)ud_catalen;

	for (ud_cat_t c = ud_catalogue; c; c = c->next) {
		ud_catobj_t dat = c->data;
		if (catobj_filter(dat, sub, slen)) {
			idx += serialise_catobj(&j->buf[idx], dat);
		}
	}

	j->blen = idx;
	return false;
}

extern bool ud_cat_cat_job(job_t j);
bool
ud_cat_cat_job(job_t j)
{
	unsigned int idx = 10;
	unsigned int slen = 0;
	char tmp[UDPC_SIMPLE_PKTLEN];
	ud_tlv_t sub[8];

	UD_DEBUG_CAT("ls job\n");
	/* filter what the luser sent us */
	slen = sort_params(sub, tmp, j);
	/* we are a seqof(UDPC_TYPE_CATOBJ) */
	j->buf[8] = (udpc_type_t)UDPC_TYPE_SEQOF;
	/* we are ud_catalen entries wide */
	j->buf[9] = (uint8_t)ud_catalen;

	for (ud_cat_t c = ud_catalogue; c; c = c->next) {
		ud_catobj_t dat = c->data;
		if (catobj_filter(dat, sub, slen)) {
			idx += serialise_catobj(&j->buf[idx], dat);
		}
	}

	j->blen = idx;
	return false;
}
#endif	/* UNSERSRV */

uint8_t
ud_fprint_tlv(const char *buf, void *file)
{
	ud_tag_t t;
	uint8_t len;
	FILE *fp = file;

	switch ((t = buf[0])) {
	case UD_TAG_CLASS:
		fputs(":class ", fp);
		len = buf[1];
		ud_fputs(len, buf + 2, fp);
		len += 2;
		break;

	case UD_TAG_NAME:
		fputs(":name ", fp);
		len = buf[1];
		ud_fputs(len, buf + 2, fp);
		len += 2;
		break;

	case UD_TAG_GROUP0_NAME:
		fputs(":g0-name ", fp);
		len = buf[1];
		ud_fputs(len, buf + 2, fp);
		len += 2;
		break;

	case UD_TAG_GROUP0_CFI:
		fputs(":g0-cfi ", fp);
		ud_fputs(sizeof(pfack_10962_t), buf + 1, fp);
		len = 1 + sizeof(pfack_10962_t);
		break;

	case UD_TAG_GROUP0_OPOL:
		fputs(":g0-opol ", fp);
		ud_fputs(sizeof(pfack_10383_t), buf + 1, fp);
		len = 1 + sizeof(pfack_10383_t);
		break;

	case UD_TAG_GROUP0_GAID: {
		instr_id_t tmp = *(const instr_id_t*const)&buf[1];
		fprintf(fp, ":g0-gaid %08x", tmp);
		len = 1 + sizeof(tmp);
		break;
	}

	case UD_TAG_GROUP2_FUND_INSTR: {
		instr_uid_t tmp = *(const instr_uid_t*const)&buf[1];
		fprintf(fp, ":g2-fund-instr %08x", (unsigned int)tmp.dummy);
		len = 1 + sizeof(tmp);
		break;
	}
	case UD_TAG_GROUP2_SETD_INSTR: {
		instr_uid_t tmp = *(const instr_uid_t*const)&buf[1];
		fprintf(fp, ":g2-setd-instr %08x", (unsigned int)tmp.dummy);
		len = 1 + sizeof(tmp);
		break;
	}

	case UD_TAG_GROUP3_ISSUE: {
		ffff_date_dse_t tmp = *(const ffff_date_dse_t*const)&buf[1];
		fprintf(fp, ":g3-isse %u", tmp);
		len = 1 + sizeof(tmp);
		break;
	}
	case UD_TAG_GROUP3_EXPIRY: {
		ffff_date_dse_t tmp = *(const ffff_date_dse_t*const)&buf[1];
		fprintf(fp, ":g3-expiry %u", tmp);
		len = 1 + sizeof(tmp);
		break;
	}
	case UD_TAG_GROUP3_SETTLE: {
		ffff_date_dse_t tmp = *(const ffff_date_dse_t*const)&buf[1];
		fprintf(fp, ":g3-settle %u", tmp);
		len = 1 + sizeof(tmp);
		break;
	}

	case UD_TAG_GROUP4_UNDERLYER: {
		unsigned int tmp = *(const unsigned int*const)&buf[1];
		fprintf(fp, ":g4-underlyer %08x", tmp);
		len = 1 + sizeof(tmp);
		break;
	}
	case UD_TAG_GROUP4_STRIKE: {
		monetary32_t tmp = *(const monetary32_t*const)&buf[1];
		fprintf(fp, ":g4-strike %2.4f", ffff_monetary_d(tmp));
		len = 1 + sizeof(tmp);
		break;
	}
	case UD_TAG_GROUP4_RATIO: {
		ratio16_t tmp = *(const ratio16_t*const)&buf[1];
		fprintf(fp, ":g4-ratio %u:%u",
			ffff_ratio16_numer(tmp), ffff_ratio16_denom(tmp));
		len = 1 + sizeof(tmp);
		break;
	}
	case UD_TAG_UNK:
	default:
		fprintf(fp, ":key %02x", t);
		len = 1;
		break;
	}
	fputc(' ', fp);
	return len;
}

/* catalogue.c ends here */
