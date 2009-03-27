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

/* our format structs */
typedef struct e123_fmt_s *e123_fmt_t;
typedef struct e123_fmtvec_s *e123_fmtvec_t;

struct e123_fmt_s {
	uint8_t idclen;
	uint8_t ndclen;
	uint8_t grplen[6];
	char idc[8];
	char ndc[8];
	bool future:1;
	bool defunct:1;
	bool trunk_prefix_dropped_p:1;
};

struct e123_fmtvec_s {
	size_t nfmts;
	struct e123_fmt_s fmts[] __attribute__((aligned(16)));
};


static int
__nfmts(const char *spec, size_t len)
{
	uint8_t cnt = 0;
	for (uint8_t i = 0; i < len; i++) {
		if (spec[i] == '[') {
			cnt++;
		}
	}
	return cnt;
}

static void
__fill_in_spec(e123_fmt_t fmt, const char *key, const char *spec)
{
	/* let all default to 0 */
	memset(fmt, 0, sizeof(*fmt));

	/* check if it's a future thing */
	if (spec[1] >= 'f' && spec[3] == 't') {
		/* future */
		fmt->future = true;
		return;
	} else if (spec[1] == 'd' && spec[3] == 'f') {
		/* defunct */
		fmt->defunct = true;
		return;
	} else if (spec[1] == 'u' && spec[3] == 'k') {
		/* unknown */
		return;
	}

	/* assert(spec[0] == '[') */
	fmt->idclen = spec[1] - '0';
	/* assert(spec[2] == ' ') */
	fmt->ndclen = spec[3] - '0';
	/* rest */
	for (int i = 5, j = 0; spec[i - 1] == ' '; i += 2, j++) {
		fmt->grplen[j] = spec[i] - '0';
	}

	/* copy the idc and ndc from KEY */
	memcpy(fmt->idc, key + 1, fmt->idclen);
	memcpy(fmt->ndc, key + 1 + fmt->idclen, fmt->ndclen);
	return;
}

static e123_fmtvec_t
make_match(const char *key, const char *spec, size_t len)
{
	e123_fmtvec_t res = NULL;

	if (spec[0] == '[') {
		/* most common case, 1 format spec */
		res = malloc(aligned_sizeof(struct e123_fmtvec_s) +
			     aligned_sizeof(struct e123_fmt_s));
		res->nfmts = 1;
		__fill_in_spec(res->fmts, key, spec);

	} else if (spec[0] == '(') {
		/* make room for NUM format specs */
		int num = __nfmts(spec, len);
		res = malloc(aligned_sizeof(struct e123_fmtvec_s) +
			     num * aligned_sizeof(struct e123_fmt_s));
		res->nfmts = num;
		/* and parse them all */
		for (uint8_t i = 0, j = 0; i < len && j < num; i++) {
			if (spec[i] == '[') {
				__fill_in_spec(&res->fmts[j++], key, &spec[i]);
			}
		}
	}
	return res;
}

static void
read_trie_line(cp_trie *t, FILE *f)
{
	char key[32], val[64];
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
		cp_trie_add(t, key, make_match(key, val, i));
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


static uint8_t
__e123ify_fmt(char *restrict resbuf, const char *inbuf, e123_fmt_t fmt)
{
	resbuf[0] = fmt->idclen + fmt->ndclen +
		fmt->grplen[0] + fmt->grplen[1] +
		fmt->grplen[2] + fmt->grplen[3] +
		fmt->grplen[4] + fmt->grplen[5];
	return 1;
}

static uint8_t
__e123ify_idc(char *restrict resbuf, const char *prefix, e123_fmt_t fmt)
{
	uint8_t idclen = fmt->idclen;

	/* IDC (international dialling code) */
	resbuf[0] = UDPC_TYPE_STRING;
	resbuf[1] = idclen;
	memcpy(&resbuf[2], fmt->idc, idclen);
	return idclen + 2;
}

static uint8_t
__e123ify_ndc(char *restrict resbuf, const char *prefix, e123_fmt_t fmt)
{
	uint8_t ndclen = fmt->ndclen;

	/* IDC (international dialling code) */
	resbuf[0] = UDPC_TYPE_STRING;
	resbuf[1] = ndclen;
	memcpy(&resbuf[2], fmt->ndc, ndclen);
	return ndclen + 2;
}

static uint8_t
__e123ify_grp(char *restrict resbuf, const char *prefix, e123_fmt_t fmt)
{
	uint8_t res = 0;
	for (res = 0; fmt->grplen[res]; res++) {
		resbuf[res] = fmt->grplen[res];
	}
	return res;
}

static uint8_t
__e123ify_1(char *restrict resbuf, const char *prefix, e123_fmt_t fmt)
{
	uint8_t len = 0, idclen, ndclen;
	char *rb = resbuf, *idc, *ndc;
	char inbuf[16];

	/* if format is unknown or future, refuse to do anything */
	if (fmt->future || fmt->defunct || fmt->idclen == 0) {
		return 0;
	}

	/* back up the prefix */
	memcpy(inbuf, prefix, sizeof(inbuf));

#define NUMLEN_OFFSET	2
	/* put in the length of the phone number we're talking */
	len += __e123ify_fmt(&resbuf[NUMLEN_OFFSET], inbuf, fmt);

#define IDC_OFFSET	NUMLEN_OFFSET + 1
	/* IDC (international dialling code) */
	rb = idc = &resbuf[IDC_OFFSET];
	len += (idclen = __e123ify_idc(rb, inbuf, fmt));

#define NDC_OFFSET	IDC_OFFSET + idclen
	/* NDC (national dialling code) */
	rb = ndc = &resbuf[NDC_OFFSET];
	len += (ndclen = __e123ify_ndc(rb, inbuf, fmt));

#define GRP_OFFSET	NDC_OFFSET + ndclen
	/* just put in the group lengths */
	rb = &resbuf[GRP_OFFSET];
	len += __e123ify_grp(rb, inbuf, fmt);

	/* announce the overall cell */
	resbuf[0] = UDPC_TYPE_VOID;
	resbuf[1] = len;
	return len + NUMLEN_OFFSET;
}

#if defined UNNAUGHTIFY
static bool __attribute__((noinline)) /* hopefully rarely called */
unnaughtify(char *inbuf)
{
	uint8_t i;

	/* peek 8 digits into it */
	for (i = 2; i < 8; i++) {
		if (inbuf[i] == '0') {
			goto move;
		}
	}
	return false;
move:
	for (char *p = &inbuf[i]; *p; p++) {
		p[0] = p[1];
	}
	return true;
}
#endif	/* UNNAUGHTIFY */


/* dump tries */
typedef cp_trie_node *trie_node_t;
static void
__dump_trie_node(char *restrict resbuf, trie_node_t node, const char *pre)
{
	mtab_node *map_node;

	/* brag about ourself first */
	fprintf(stderr, "%s%s\n", pre, node->leaf ? node->leaf : "");
	for (int i = 0; i < node->others->size; i++) {
		map_node = node->others->table[i];
		while (map_node) {
			__dump_trie_node(resbuf, map_node->value, map_node->attr);
			map_node = map_node->next;
		}
	}
	return;
}

static void
dump_trie(char *restrict resbuf)
{
	fprintf(stderr, "dumping\n");
	__dump_trie_node(resbuf, loc_trie->root, "");
	return;
}


/* public job fun, as announced in unserding-private.h */
static uint8_t
ud_5e_e123ify_job(char *restrict resbuf, /*const*/ char *inbuf)
{
	e123_fmtvec_t fmtvec;
	uint16_t totlen = 0;

	/* query for the number */
	cp_trie_prefix_match(loc_trie, inbuf, (void*)&fmtvec);

#if defined UNNAUGHTIFY
	/* trivial cases first */
	if (UNLIKELY(fmtvec == NULL && unnaughtify(inbuf))) {
		/* query for the number */
		cp_trie_prefix_match(loc_trie, inbuf, (void*)&fmtvec);
	}
#endif	/* UNNAUGHTIFY */
	if (UNLIKELY(fmtvec == NULL)) {
		/* return a not-found? */
		return 0;
	}
	/* sequence of possible format info */
	resbuf[0] = UDPC_TYPE_SEQOF;
	/* the number of info */
	resbuf[1] = fmtvec->nfmts;
	/* e123ify at least once */
	resbuf += (totlen = __e123ify_1(resbuf + 2, inbuf, fmtvec->fmts));
	/* traverse the rest */
	for (uint8_t i = 1; i < fmtvec->nfmts; i++) {
		totlen += __e123ify_1(resbuf + 2, inbuf, &fmtvec->fmts[i]);
		resbuf += totlen;
	}
	return totlen + 2;
}

#define PAYLOAD_OFFSET	8
#define WS_OFFSET	10

/* service stuff */
static void
f5e_e123ify(job_t j)
{
	uint8_t pktlen;
	char *restrict rb = &j->buf[PAYLOAD_OFFSET];
	char *ib = &j->buf[WS_OFFSET];

	if (UNLIKELY(rb[0] != UDPC_TYPE_STRING)) {
		dump_trie(rb);
		return;
	}

	/* generate the answer packet */
	udpc_make_rpl_pkt(JOB_PACKET(j));

	if ((pktlen = ud_5e_e123ify_job(rb, ib)) == 0) {
		return;
	}
	/* compute the overall length */
	j->blen = PAYLOAD_OFFSET + pktlen;

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
		loc_trie = NULL;
		return;
	}
	/* put it into fam5e array */
	ud_fam5e[2] = f5e_e123ify;
	return;
}

void
mod_e123_LTX_deinit(void)
{
	if (LIKELY(loc_trie != NULL)) {
		cp_trie_destroy(loc_trie);
	}
	return;
}

/* mod-e123.c ends here */
