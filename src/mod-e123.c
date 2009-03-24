/*** mod-e123.c -- unserding module to check for e.123 compliance
 *
 * Copyright (C) 2009 Sebastian Freundt
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

/* cprops helper, planned on getting rid of this */
#include <cprops/trie.h>

/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"

extern void mod_e123_LTX_init(void);
extern void mod_e123_LTX_deinit(void);

/* our local trie */
static cp_trie *loc_trie;


static void
read_trie_line(cp_trie *t, FILE *f)
{
	char key[32], val[32];
	char c;
	int i;

	for (i = 0; (c = fgetc(f)) != '\t' && c != EOF; i++) {
		key[i] = c;
	}
	key[i] = '\0';

	for (i = 0; (c = fgetc(f)) != '\n' && c != EOF; i++) {
		val[i] = c;
	}
	val[i] = '\0';

	if (c != EOF) {
		cp_trie_add(t, key, __builtin_strndup(val, i+1));
	}
	return;
}

static void __attribute__((noinline))
build_trie(cp_trie *t, const char *file)
{
	FILE *f;

	if ((f = fopen(file, "r")) == NULL) {
		return;
	}

	while (!feof(f)) {
		read_trie_line(t, f);
	}

	fclose(f);
	return;
}


/* reformat the inbuf */
static uint8_t
__e123ify(char *restrict resbuf, const char *inbuf, void *match)
{
	size_t inblen = strlen(inbuf);
	char tmpin[256], *mp = match;
	uint8_t ii = 0, ri = 0;

	if (UNLIKELY(match == NULL)) {
		return 0;
	}
	if (*mp++ != '[') {
		return 0;
	}

#define WILDCARD	'#'
	/* buffer inbuf temporarily, assume stripped string */
	memset(tmpin, WILDCARD, sizeof(tmpin));
	memcpy(tmpin, inbuf, inblen);

	/* go through the format specs */
	/* copy the initial '+' */
	resbuf[ri++] = tmpin[ii++];

	do {
		for (int i = 0, n = *mp++ - '0'; i < n; i++) {
			resbuf[ri++] = tmpin[ii++];
		}
		resbuf[ri++] = ' ';
		++mp;
	} while ((*mp >= '0' && *mp <= '9') ||
		 /* always false */
		 (resbuf[--ri] = '\0'));
	/* copy the rest */
	while (ii < inblen) {
		resbuf[ri++] = tmpin[ii++];
	}
	return ri;
}


/* public job fun, as announced in unserding-private.h */
uint8_t
ud_5e_e123ify_job(char *restrict resbuf, /*const*/ char *inbuf)
{
	void *match;

	/* query for the number */
	cp_trie_prefix_match(loc_trie, inbuf, &match);
	return __e123ify(resbuf, inbuf, match);
}

/* service stuff */
static void
f5e_e123ify(job_t j)
{
	/* generate the answer packet */
	udpc_make_rpl_pkt(JOB_PACKET(j));

	/* we're a string, soon will be a seqof(string) */
	j->buf[8] = UDPC_TYPE_STRING;
	/* attach the hostname now */
	j->buf[9] = ud_5e_e123ify_job(&j->buf[10], &j->buf[10]);
	/* compute the overall length */
	j->blen = 8 + 2 + j->buf[9];

	UD_DEBUG_PROTO("sending 5e/02 RPL\n");
	/* and send him back */
	send_cl(j);
	return;
}


extern ud_pktwrk_f ud_fam5e[];

void
mod_e123_LTX_init(void)
{
	if (LIKELY((loc_trie = cp_trie_create(0)) != NULL)) {
		/* hm, where are we gonna store this?
		 * ultimately in a database! */
		build_trie(loc_trie, "/tmp/all-city-codes");
	} else {
		UD_CRITICAL("can\'t create trie\n");
	}
	/* put it into fam5e array */
	ud_fam5e[2] = f5e_e123ify;
	return;
}

void
mod_e123_LTX_deinit(void)
{
	if (LIKELY((loc_trie = cp_trie_create(0)) != NULL)) {
		cp_trie_destroy(loc_trie);
	}
	return;
}

/* mod-e123.c ends here */
