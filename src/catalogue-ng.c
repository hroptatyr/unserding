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


static unsigned int
ud_write_g0_name(char *restrict buf, const char *name)
{
	size_t len = strlen(name);
	if (UNLIKELY(len == 0)) {
		return 0;
	}
	/* otherwise */
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_NAME;
	buf[2] = (uint8_t)len;
	memcpy(buf + 3, name, len);
	return len + 3;
}

static unsigned int
ud_write_g0_cfi(char *restrict buf, const char *cfi)
{
	/* otherwise */
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_CFI;
	memcpy(buf + 2, cfi, sizeof(pfack_10962_t));
	return sizeof(pfack_10962_t) + 2;
}

static unsigned int
ud_write_g0_opol(char *restrict buf, const char *opol)
{
	/* otherwise */
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_OPOL;
	memcpy(buf + 2, opol, sizeof(pfack_10383_t));
	return sizeof(pfack_10383_t) + 2;
}

static unsigned int
ud_write_g0_gaid(char *restrict buf, instr_id_t gaid)
{
	/* otherwise */
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_GAID;
	memcpy(buf + 2, (char*)&gaid, sizeof(instr_id_t));
	return sizeof(instr_id_t) + 2;
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
		instr_grp_general_t grp = tmp;
		idx += ud_write_g0_name(&buf[idx], grp->name);
		idx += ud_write_g0_cfi(&buf[idx], grp->cfi);
		idx += ud_write_g0_opol(&buf[idx], grp->opol);
		idx += ud_write_g0_gaid(&buf[idx], grp->ga_id);
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
