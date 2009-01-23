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

#if defined UNSERSRV
typedef struct ud_cat_s *__cat_t;

static const char empty_msg[] = "empty\n";

/* the global catalogue */
static ud_cat_t ud_catalogue = NULL;
static size_t ud_catalen = 0;


/* helpers */
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

static inline size_t __attribute__((always_inline))
snprintcat(char *restrict buf, size_t blen, const __cat_t c)
{
	size_t len = snprintf(buf, blen, "---- %p %s\n",
			      c->data, (const char*)c->data);
#if 0
	if (ud_cat_justcatp(c)) {
		buf[3] = 'c';
	}
	if (ud_cat_spottablep(c)) {
		buf[2] = 's';
	}
	if (ud_cat_tradablep(c)) {
		buf[1] = 't';
	}
	if (ud_cat_lastp(c)) {
		buf[0] = 'l';
	}
#endif
	return len;
}

static size_t
serialise_keyval(char *restrict buf, ud_tlv_t keyval)
{
	size_t idx = 2;
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

static size_t
serialise_catobj(char *restrict buf, ud_catobj_t co)
{
	size_t idx = 2;

	/* we are a UDPC_TYPE_CATOBJ */
	buf[0] = (udpc_type_t)UDPC_TYPE_CATOBJ;
	buf[1] = co->nattrs;
	for (uint8_t i = 0; i < co->nattrs; ++i) {
		idx += serialise_keyval(&buf[idx], co->attrs[i]);
	}
	return idx;
}

/* some jobs to browse the catalogue */
extern bool ud_cat_ls_job(job_t j);
bool
ud_cat_ls_job(job_t j)
{
	size_t idx = 10;

	UD_DEBUG_CAT("ls job\n");
	/* we are a seqof(UDPC_TYPE_CATOBJ) */
	j->buf[8] = (udpc_type_t)UDPC_TYPE_SEQOF;
	/* we are ud_catalen entries wide */
	j->buf[9] = (uint8_t)ud_catalen;

	for (ud_cat_t c = ud_catalogue; c; c = c->next) {
		ud_catobj_t dat = c->data;
		idx += serialise_catobj(&j->buf[idx], dat);
	}

	j->blen = idx;
	return false;
}
#endif	/* UNSERSRV */

uint8_t
ud_fprint_tlv(const char *buf, FILE *fp)
{
	ud_tag_t t;
	uint8_t len;

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

	case UD_TAG_UNK:
	default:
		fprintf(fp, ":key %02x", t);
		len = 1;
		break;
	}
	fputc(' ', fp);
	return len;
}

/**
 * For a trie unify ud_tag_t and a pointer to an array thereof. */
typedef const union __tag_trie_u *__tag_trie_t;
union __tag_trie_u {
	ud_tag_t tag;
	__tag_trie_t trie;
	char offs;
};


#if defined USE_TRIES || 1
/* the trie approach is about factor 10 faster than the naive approach below
 * however, memcmp is not totally useless, we could do sorting with it. */

static const union __tag_trie_u ctrie[] = {
	/* default return */
	{UD_TAG_UNK},
	/* offset */
	{.offs = 'l'},
	/* l */
	{UD_TAG_CLASS},
	/* m */
	{UD_TAG_UNK},
	/* n */
	{UD_TAG_UNK},
	/* o */
	{UD_TAG_UNK},
	/* p */
	{UD_TAG_UNK},
	/* q */
	{UD_TAG_UNK},
	/* r */
	{UD_TAG_UNK},
	/* s */
	{UD_TAG_UNK},
	/* t */
	{UD_TAG_UNK},
	/* u */
	{UD_TAG_CURRENCY},
};
static const union __tag_trie_u strie[] = {
	/* default return */
	{UD_TAG_UNK},
	/* offset */
	{.offs = 't'},
	/* t */
	{UD_TAG_STRIKE},
	/* u */
	{UD_TAG_UNK},
	/* v */
	{UD_TAG_UNK},
	/* w */
	{UD_TAG_UNK},
	/* x */
	{UD_TAG_UNK},
	/* y */
	{UD_TAG_SYMBOL},
};
static const union __tag_trie_u ptrie[] = {
	/* default return */
	{UD_TAG_UNK},
	/* offset */
	{.offs = 'a'},
	/* a */
	{UD_TAG_PADDR},
	/* b */
	{UD_TAG_UNK},
	/* c */
	{UD_TAG_UNK},
	/* d */
	{UD_TAG_UNK},
	/* e */
	{UD_TAG_UNK},
	/* f */
	{UD_TAG_UNK},
	/* g */
	{UD_TAG_UNK},
	/* h */
	{UD_TAG_UNK},
	/* i */
	{UD_TAG_UNK},
	/* j */
	{UD_TAG_UNK},
	/* k */
	{UD_TAG_UNK},
	/* l */
	{UD_TAG_PLACE},
	/* m */
	{UD_TAG_UNK},
	/* n */
	{UD_TAG_UNK},
	/* o */
	{UD_TAG_UNK},
	/* p */
	{UD_TAG_UNK},
	/* q */
	{UD_TAG_UNK},
	/* r */
	{UD_TAG_PRICE},
};
/* global trie */
static const union __tag_trie_u trie[] = {
	/* default return */
	{UD_TAG_UNK},
	/* offset */
	{.offs = 'a'},
	/* a */
	{UD_TAG_ATTR},
	/* b */
	{UD_TAG_UNK},
	/* c */
	{.trie = ctrie},
	/* d */
	{UD_TAG_UNK},
	/* e */
	{UD_TAG_EXPIRY},
	/* f */
	{UD_TAG_UNK},
	/* g */
	{UD_TAG_UNK},
	/* h */
	{UD_TAG_UNK},
	/* i */
	{UD_TAG_UNK},
	/* j */
	{UD_TAG_UNK},
	/* k */
	{UD_TAG_UNK},
	/* l */
	{UD_TAG_UNK},
	/* m */
	{UD_TAG_UNK},
	/* n */
	{UD_TAG_NAME},
	/* o */
	{UD_TAG_UNK},
	/* p */
	{.trie = ptrie},
	/* q */
	{UD_TAG_UNK},
	/* r */
	{UD_TAG_UNK},
	/* s */
	{.trie = strie},
	/* t */
	{UD_TAG_UNK},
	/* u */
	{UD_TAG_UNDERLYING},
	/* v */
	{UD_TAG_UNK},
	/* w */
	{UD_TAG_UNK},
	/* x */
	{UD_TAG_UNK},
	/* y */
	{UD_TAG_UNK},
	/* z */
	{UD_TAG_UNK},
};

ud_tag_t
ud_tag_from_s(const char *buf)
{
	union __tag_trie_u res;

	if (LIKELY(buf[0] == ':')) {
		buf++;
	}

	for (res.trie = trie; buf[0] != '\0'; buf++) {
		/* descend */
		res.trie = res.trie[(*buf - res.trie[1].offs + 2)].trie;
		if (((long unsigned int)res.trie & ~0x1fUL) == 0) {
			/* found a match */
			return res.tag;
		}
	}
	return UD_TAG_UNK;
}

#else  /* !USE_TRIES */

ud_tag_t
ud_tag_from_s(const char *s)
{
	if (memcmp(s, ":class", 6) == 0) {
		return UD_TAG_CLASS;
	} else if (memcmp(s, ":name", 5) == 0) {
		return UD_TAG_NAME;
	} else if (memcmp(s, ":price", 6) == 0) {
		return UD_TAG_PRICE;
	} else if (memcmp(s, ":attr", 5) == 0) {
		return UD_TAG_ATTR;
	} else if (memcmp(s, ":paddr", 6) == 0) {
		return UD_TAG_PADDR;
	} else if (memcmp(s, ":place", 6) == 0) {
		return UD_TAG_PLACE;
	} else if (memcmp(s, ":currency", 5) == 0) {
		return UD_TAG_CURRENCY;
	} else if (memcmp(s, ":symbol", 7) == 0) {
		return UD_TAG_SYMBOL;
	} else if (memcmp(s, ":strike", 7) == 0) {
		return UD_TAG_STRIKE;
	} else if (memcmp(s, ":paddr", 6) == 0) {
		return UD_TAG_PADDR;
	} else if (memcmp(s, ":underlying", 4) == 0) {
		return UD_TAG_UNDERLYING;
	} else if (memcmp(s, ":expiry", 7) == 0) {
		return UD_TAG_EXPIRY;
	}
	return UD_TAG_UNK;
}
#endif	/* USE_TRIES */

/* catalogue.c ends here */
