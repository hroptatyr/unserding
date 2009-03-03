/*** catalogue-ng.c -- new generation catalogue
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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
#include "catalogue-ng.h"
#include "protocore.h"
#include "catalogue.h"
/* other external stuff */
#include <pfack/instruments.h>
#include <ffff/hashtable.h>

extern void *instruments;

/* ctor, dtor */
catng_t
make_catalogue(void)
{
	struct ase_dict_options_s opts = {
		.initial_size = 64,
		.worst_case_constant_lookup_p = true,
		.two_power_sizes = true,
		.arity = 4,
	};
	return (void*)ase_make_htable(&opts);
}

void
free_catalogue(catng_t cat)
{
	ase_free_htable(cat);
	return;
}

/* modifiers */
void
catalogue_add_instr(catng_t cat, const instr_t instr)
{
	unsigned int cod = instr_general_group(instr)->ga_id;
	void *key = (void*)(long unsigned int)cod;
	ase_htable_put(cat, cod, key/*val-only?*/, instr);
	return;
}


/* helpers */
static inline unsigned int
ud_write_instr_uid(char *restrict buf, ud_tag_t t, instr_uid_t uid)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&uid, sizeof(instr_uid_t));
	return 2 + sizeof(uid);
}

static inline unsigned int
ud_write_date_dse(char *restrict buf, ud_tag_t t, ffff_date_dse_t d)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&d, sizeof(d));
	return sizeof(d) + 2;
}

static inline unsigned int
ud_write_monetary32(char *restrict buf, ud_tag_t t, monetary32_t m)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&m, sizeof(m));
	return sizeof(m) + 2;
}

static inline unsigned int
ud_write_short(char *restrict buf, ud_tag_t t, short int s)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&s, sizeof(s));
	return sizeof(s) + 2;
}


static unsigned int
ud_write_g0_name(char *restrict buf, const void *grp)
{
	instr_grp_general_t tmp = grp;
	size_t len = strlen(tmp->name);
	if (UNLIKELY(len == 0)) {
		return 0;
	}
	/* otherwise */
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_NAME;
	buf[2] = (uint8_t)len;
	memcpy(buf + 3, tmp->name, len);
	return len + 3;
}

static unsigned int
ud_write_g0_cfi(char *restrict buf, const void *grp)
{
	instr_grp_general_t tmp = grp;
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_CFI;
	memcpy(buf + 2, tmp->cfi, sizeof(pfack_10962_t));
	return sizeof(pfack_10962_t) + 2;
}

static unsigned int
ud_write_g0_opol(char *restrict buf, const void *grp)
{
	instr_grp_general_t tmp = grp;
	/* otherwise */
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_OPOL;
	memcpy(buf + 2, tmp->opol, sizeof(pfack_10383_t));
	return sizeof(pfack_10383_t) + 2;
}

static unsigned int
ud_write_g0_gaid(char *restrict buf, const void *grp)
{
	instr_grp_general_t tmp = grp;
	return ud_write_instr_uid(buf, UD_TAG_GROUP0_GAID, tmp->ga_id);
}

static unsigned int
ud_write_g2_fund_instr(char *restrict buf, const void *grp)
{
	instr_grp_funding_t tmp = grp;
	return ud_write_instr_uid(
		buf, UD_TAG_GROUP2_FUND_INSTR, tmp->fund_instr);
}

static unsigned int
ud_write_g2_set_instr(char *restrict buf, const void *grp)
{
	instr_grp_funding_t tmp = grp;
	return ud_write_instr_uid(
		buf, UD_TAG_GROUP2_SET_INSTR, tmp->set_instr);
}

static unsigned int
ud_write_g3_start(char *restrict buf, const void *grp)
{
	instr_grp_delivery_t tmp = grp;
	return ud_write_date_dse(buf, UD_TAG_GROUP3_START, tmp->start);
}

static unsigned int
ud_write_g3_expiry(char *restrict buf, const void *grp)
{
	instr_grp_delivery_t tmp = grp;
	return ud_write_date_dse(buf, UD_TAG_GROUP3_EXPIRY, tmp->expiry);
}

static unsigned int
ud_write_g3_settle(char *restrict buf, const void *grp)
{
	instr_grp_delivery_t tmp = grp;
	return ud_write_date_dse(buf, UD_TAG_GROUP3_SETTLE, tmp->settle);
}

static unsigned int
ud_write_g4_underlyer(char *restrict buf, const void *grp)
{
	instr_grp_referent_t tmp = grp;
	return ud_write_instr_uid(buf, UD_TAG_GROUP4_UNDERLYER, tmp->underlyer);
}

static unsigned int
ud_write_g4_strike(char *restrict buf, const void *grp)
{
	instr_grp_referent_t tmp = grp;
	return ud_write_monetary32(buf, UD_TAG_GROUP4_STRIKE, tmp->strike);
}

static unsigned int
ud_write_g4_ratio_numer(char *restrict buf, const void *grp)
{
	instr_grp_referent_t tmp = grp;
	return ud_write_short(buf, UD_TAG_GROUP4_RATIO_NUMER, tmp->ratio_numer);
}

static unsigned int
ud_write_g4_ratio_denom(char *restrict buf, const void *grp)
{
	instr_grp_referent_t tmp = grp;
	return ud_write_short(buf, UD_TAG_GROUP4_RATIO_DENOM, tmp->ratio_denom);
}


/* unserding serialiser */
static unsigned int
serialise_catobj(char *restrict buf, const_instr_t instr)
{
	unsigned int idx;
	short unsigned int grpset = instr->grps;
	char *p;
	const void *tmp;

	/* we are a UDPC_TYPE_CATOBJ */
	buf[0] = (udpc_type_t)UDPC_TYPE_PFINSTR;
	/* write the group set */
	buf[1] = (udpc_type_t)UDPC_TYPE_WORD;
	idx = 2;
	p = (char*)&grpset;
	buf[idx++] = *p++;
	buf[idx++] = *p++;

	/* encode the groups now */
	if ((tmp = instr_general_group(instr)) != NULL) {
		/* write group 0, general group */
		/* :name, :cfi, :opol, :gaid */
		idx += ud_write_g0_name(&buf[idx], tmp);
		idx += ud_write_g0_cfi(&buf[idx], tmp);
		idx += ud_write_g0_opol(&buf[idx], tmp);
		idx += ud_write_g0_gaid(&buf[idx], tmp);
	}
	if ((tmp = instr_funding_group(instr)) != NULL) {
		/* write group 2, general group */
		/* :fund-instr, :set-instr */
		idx += ud_write_g2_fund_instr(&buf[idx], tmp);
		idx += ud_write_g2_set_instr(&buf[idx], tmp);
	}
	if ((tmp = instr_delivery_group(instr)) != NULL) {
		/* write group 3, general group */
		/* :start, :expiry, :settle */
		idx += ud_write_g3_start(&buf[idx], tmp);
		idx += ud_write_g3_expiry(&buf[idx], tmp);
		idx += ud_write_g3_settle(&buf[idx], tmp);
	}
	if ((tmp = instr_referent_group(instr)) != NULL) {
		/* write group 4, general group */
		/* :underlyer, :strike, :ratio-numer, :ratio-denom */
		idx += ud_write_g4_underlyer(&buf[idx], tmp);
		idx += ud_write_g4_strike(&buf[idx], tmp);
		idx += ud_write_g4_ratio_numer(&buf[idx], tmp);
		idx += ud_write_g4_ratio_denom(&buf[idx], tmp);
	}
	return idx;
}


/* another browser */
extern bool ud_cat_lc_job(job_t j);
bool
ud_cat_lc_job(job_t j)
{
	unsigned int idx = 10;
	//unsigned int slen = 0;
	//char tmp[UDPC_SIMPLE_PKTLEN];
	//ud_tlv_t sub[8];
	struct ase_dict_iter_s iter;
	const void *key;
	void *val;

	UD_DEBUG_CAT("lc job\n");
	/* filter what the luser sent us */
	//slen = sort_params(sub, tmp, j);
	/* we are a seqof(UDPC_TYPE_CATOBJ) */
	j->buf[8] = (udpc_type_t)UDPC_TYPE_SEQOF;
	/* we are ud_catalen entries wide */
	j->buf[9] = (uint8_t)ase_htable_fill(instruments);

	ht_iter_init_ll(instruments, &iter);
	while (ht_iter_next(&iter, &key, &val)) {
		const_instr_t instr = val;
		/* filter me one day
		 * for now we just spill what we've got */
		idx += serialise_catobj(&j->buf[idx], instr);
	}
	ht_iter_fini_ll(&iter);

	j->blen = idx;
	return false;
}

/* catalogue-ng.c ends here */
