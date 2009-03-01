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
		.initial_size = 32,
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
catalogue_add_instr(catng_t cat, const instrument_t instr, hcode_t cod)
{
	void *key = (void*)cod;
	ase_htable_put(cat, cod, key/*val-only?*/, instr);
	return;
}


static unsigned int
serialise_catobj(char *restrict buf, const void *key, const_instrument_t instr)
{
	unsigned int idx;
	unsigned int cod = (unsigned int)(long unsigned int)key;

	/* we are a UDPC_TYPE_CATOBJ */
	buf[0] = (udpc_type_t)UDPC_TYPE_PFINSTR;
	idx = 1;
	/* spit out the code */
	buf[idx++] = (udpc_type_t)UDPC_TYPE_DWORD;
	buf[idx++] = ((char*)&cod)[0];
	buf[idx++] = ((char*)&cod)[1];
	buf[idx++] = ((char*)&cod)[2];
	buf[idx++] = ((char*)&cod)[3];

	/* use libpfack's serialiser */
	idx += seria_instrument(&buf[idx], 4096, instr);
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
		const_instrument_t instr = val;
		/* filter me one day
		 * for now we just spill what we've got */
		idx += serialise_catobj(&j->buf[idx], key, instr);
	}
	ht_iter_fini_ll(&iter);

	j->blen = idx;
	return false;
}

/* catalogue-ng.c ends here */
