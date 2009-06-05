/*** dso-bbdb.c -- BBDBng
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <hroptatyr@sxemacs.org>
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "module.h"
#include "unserding.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include <sys/param.h>

#include "protocore.h"

#include <libxml/parser.h>

#define xnew(_x)	malloc(sizeof(_x))

typedef size_t index_t;

typedef void *bbdb_t;
typedef struct entry_s *entry_t;

static bbdb_t recs = NULL;

struct entry_s {
	entry_t next;
	const char *fn;
	size_t fnlen;
};


static void sta(void*, const xmlChar*, const xmlChar**);
static void end(void*, const xmlChar*);

static xmlSAXHandler bbdb_handler = {
	.startElement = sta,
	.endElement = end,
};

typedef struct bbdb_pctx_s *bbdb_pctx_t;
struct bbdb_pctx_s {
	entry_t entry;
};

static void
init_new_entry(void *ctx)
{
	bbdb_pctx_t pctx = ctx;
	pctx->entry = xnew(*pctx->entry);
	printf("new entry...");
	return;
}

static void
fini_new_entry(void *ctx)
{
	bbdb_pctx_t pctx = ctx;
	printf("done\n");
	pctx->entry->next = recs;
	recs = pctx->entry;
	pctx->entry = NULL;
	return;
}

static void
parse_fn(void *ctx, const xmlChar *ch, int len)
{
	bbdb_pctx_t pctx = ctx;
	char *restrict *fn =
		(void*)((char*)pctx->entry + offsetof(struct entry_s, fn));

	pctx->entry->fn = malloc(len+1);
	strncpy(*fn, (const char*)ch, len);
	pctx->entry->fnlen = len;
	fputs((const char*)pctx->entry->fn, stdout);
	return;
}

static void
sta(void *ctx, const xmlChar *name, const xmlChar **attrs)
{
	if (strcmp((const char*)name, "entry") == 0) {
		init_new_entry(ctx);
	} else if (strcmp((const char*)name, "fullname") == 0) {
		bbdb_handler.characters = parse_fn;
	}
	return;
}

static void
end(void *ctx, const xmlChar *name)
{
	if (strcmp((const char*)name, "entry") == 0) {
		fini_new_entry(ctx);
	} else if (strcmp((const char*)name, "fullname") == 0) {
		bbdb_handler.characters = NULL;
	}
	return;
}

static void
parse_file(const char *filename)
{
	struct bbdb_pctx_s pctx;
	if (xmlSAXUserParseFile(&bbdb_handler, &pctx, filename) < 0) {
		return;
	}
	return;
}

static int
read_bbdb(void)
{
	const char bbdbfn[] = "/.bbdb.xml";
	char full[MAXPATHLEN], *tmp;

	/* construct the name */
	tmp = stpcpy(full, getenv("HOME"));
	strncpy(tmp, bbdbfn, sizeof(bbdbfn));

	parse_file(full);
	return 0;
}

static const char*
boyer_moore(const char *buf, size_t buflen, const char *pat, size_t patlen)
{
	index_t k;
	long int next[UCHAR_MAX];
	long int skip[UCHAR_MAX];

	if (patlen > buflen || patlen >= UCHAR_MAX) {
		return NULL;
	}

	/* calc skip table ("bad rule") */
	for (index_t i = 0; i <= UCHAR_MAX; i++) {
		skip[i] = patlen;
	}
	for (index_t i = 0; i < patlen; i++) {
		skip[(int)pat[i]] = patlen - i - 1;
	}

	for (index_t j = 0, i; j <= patlen; j++) {
		for (i = patlen - 1; i >= 1; i--) {
			for (k = 1; k <= j; k++) {
				if (i - k < 0) {
					goto matched;
				}
				if (pat[patlen - k] != pat[i - k]) {
					goto nexttry;
				}
			}
			goto matched;
		nexttry: ;
		}
	matched:
		next[j] = patlen - i;
	}

	patlen--;

	for (index_t i = patlen; i < buflen; ) {
		for (index_t j = 0 /* matched letter count */; j <= patlen; ) {
			if (buf[i - j] == pat[patlen - j]) {
				j++;
				continue;
			}
			i += skip[(int)buf[i - j]] > next[j]
				? skip[(int)buf[i - j]]
				: next[j];
			goto newi;
		}
		return buf + i - patlen;
	newi:
		;
	}
	return NULL;
}

static entry_t
find_entry(const char *str, size_t len)
{
	/* traverse our entries */
	for (entry_t r = recs; r; r = r->next) {
		if (boyer_moore(r->fn, r->fnlen, str, len) != NULL) {
			return r;
		}
	}
	return NULL;
}

static void
bbdb_seria_fullname(udpc_seria_t sctx, entry_t rec)
{
	const char *s = rec->fn;
	size_t sl = rec->fnlen;
	udpc_seria_add_str(sctx, s, sl);
	return;
}


/* #'bbdb-search */
static void
bbdb_search(job_t j)
{
	struct udpc_seria_s sctx;
	size_t ssz;
	const char *sstr;
	entry_t rec;
	static const char nmfld[] = "fullname";

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	ssz = udpc_seria_des_str(&sctx, &sstr);
	UD_DEBUG("mod/bbdb: <- search \"%s\"\n", sstr);

	if (ssz == 0) {
		/* actually means dump all of it */
		return;
	}

	if ((rec = find_entry(sstr, ssz)) == NULL) {
		return;
	}

	udpc_make_rpl_pkt(JOB_PACKET(j));
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* serialise the fullname */
	udpc_seria_add_str(&sctx, nmfld, sizeof(nmfld)-1);
	bbdb_seria_fullname(&sctx, rec);

	/* chop chop, off we go */
	j->blen = UDPC_HDRLEN + udpc_seria_msglen(&sctx);
	send_cl(j);
	return;
}


void
init(void *clo)
{
	UD_DEBUG("mod/bbdb: loading ...");

	if (read_bbdb() == 0) {
		/* lodging our bbdb search service */
		ud_set_service(0xbbda, bbdb_search, NULL);
		UD_DBGCONT("done\n");

	} else {
		UD_DBGCONT("failed\n");
	}
	return;
}

void
reinit(void *clo)
{
	UD_DEBUG("mod/bbdb: reloading ...");

	if (read_bbdb() == 0) {
		/* lodging our bbdb search service */
		ud_set_service(0xbbda, bbdb_search, NULL);
		UD_DBGCONT("done\n");

	} else {
		UD_DBGCONT("failed\n");
	}
	return;
}

void
deinit(void *clo)
{
	UD_DEBUG("mod/bbdb: unloading ...");
	/* clearing bbdb search service */
	ud_set_service(0xbbda, NULL, NULL);
	UD_DBGCONT("done\n");
	return;
}

/* dso-bbdb.c ends here */
