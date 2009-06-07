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

#include "bbdb-mulvalbuf.h"

#define xnew(_x)	malloc(sizeof(_x))

typedef size_t index_t;

typedef void *bbdb_t;
typedef struct entry_s *entry_t;

static bbdb_t glob_recs = NULL;

struct entry_s {
	entry_t next;
	const char *fn;
	size_t fnlen;
	struct mvbuf_s akas;
	struct mvbuf_s emails;
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
	memset(pctx->entry, 0, sizeof(*pctx->entry));
	printf("new entry...");
	fflush(stdout);
	return;
}

static void
fini_new_entry(void *ctx)
{
	bbdb_pctx_t pctx = ctx;
	printf("done\n");
	pctx->entry->next = glob_recs;
	glob_recs = pctx->entry;
	pctx->entry = NULL;
	return;
}

static void
parse_fn(void *ctx, const xmlChar *ch, int len)
{
	bbdb_pctx_t pctx = ctx;
	char **fn = (void*)((char*)pctx->entry + offsetof(struct entry_s, fn));

	pctx->entry->fn = malloc(len+1);
	strncpy(*fn, (const char*)ch, len);
	pctx->entry->fnlen = len;
	(*fn)[len] = '\0';
	fputs((const char*)pctx->entry->fn, stdout);
	return;
}

static void
parse_email(void *ctx, const xmlChar *ch, int len)
{
	bbdb_pctx_t pctx = ctx;

	mvbuf_add(&pctx->entry->emails, (const char*)ch, len);
	return;
}

static void
parse_aka(void *ctx, const xmlChar *ch, int len)
{
	bbdb_pctx_t pctx = ctx;

	mvbuf_add(&pctx->entry->akas, (const char*)ch, len);
	return;
}

static void
sta(void *ctx, const xmlChar *name, const xmlChar **attrs)
{
	if (strcmp((const char*)name, "entry") == 0) {
		init_new_entry(ctx);
	} else if (strcmp((const char*)name, "fullname") == 0) {
		bbdb_handler.characters = parse_fn;
	} else if (strcmp((const char*)name, "email") == 0) {
		bbdb_handler.characters = parse_email;
	} else if (strcmp((const char*)name, "aka") == 0) {
		bbdb_handler.characters = parse_aka;
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
	} else if (strcmp((const char*)name, "email") == 0) {
		bbdb_handler.characters = NULL;
	} else if (strcmp((const char*)name, "aka") == 0) {
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

static inline char
icase(char c)
{
	if (c >= 'A' && c <= 'Z') {
		return c | 0x60;
	}
	return c;
}

static inline bool
icase_eqp(char c1, char c2)
{
	if (c1 >= 'A' && c1 <= 'Z' && (c1 | 0x60) == c2) {
		return true;
	}
	return c1 == c2;
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
			if (icase_eqp(buf[i - j], pat[patlen - j])) {
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
find_entry(const char *str, size_t len, bbdb_t recs)
{
	/* traverse our entries */
	for (entry_t r = recs; r; r = r->next) {
		if (boyer_moore(r->fn, r->fnlen, str, len) != NULL) {
			return r;
		}
		if (boyer_moore(mvbuf_buffer(&r->emails),
				mvbuf_buffer_len(&r->emails),
				str, len) != NULL) {
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


static inline void
prep_pkt(udpc_seria_t sctx, job_t j)
{
	udpc_make_rpl_pkt(JOB_PACKET(j));
	udpc_seria_init(sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	return;
}

static inline void
send_pkt(udpc_seria_t sctx, job_t j)
{
	j->blen = UDPC_HDRLEN + udpc_seria_msglen(sctx);
	send_cl(j);
	return;
}

/* #'bbdb-search */
static void
bbdb_search(job_t j)
{
	struct udpc_seria_s sctx;
	size_t ssz;
	const char *sstrp;
	char sstr[256];
	entry_t rec = glob_recs;
	static const char nmfld[] = "fullname";
	static const char emfld[] = "email";
	static const char akfld[] = "aka";

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	ssz = udpc_seria_des_str(&sctx, &sstrp);
	UD_DEBUG("mod/bbdb: <- search \"%s\"\n", sstrp);

	if (ssz == 0) {
		/* actually means dump all of it */
		return;
	}

	/* copy him, so we can start wiping the packet */
	memcpy(sstr, sstrp, ssz);

	/* prepare the job packet */
	prep_pkt(&sctx, j);
	for (; (rec = find_entry(sstr, ssz, rec)); rec = rec->next) {
		mvbuf_t em = &rec->emails;
		mvbuf_t ak = &rec->akas;

		if (udpc_seria_msglen(&sctx) +
		    rec->fnlen + mvbuf_buffer_len(em) > UDPC_PLLEN - 100) {
			/* chop chop, off we go */
			send_pkt(&sctx, j);
			/* prepare another job packet */
			prep_pkt(&sctx, j);
		}
		/* serialise the fullname */
		udpc_seria_add_str(&sctx, nmfld, sizeof(nmfld)-1);
		bbdb_seria_fullname(&sctx, rec);
		/* serialise the emails */
		for (mvbsize_t idx = 0; idx < mvbuf_nvals(ak); idx++) {
			const char *s;
			mvbsize_t len = mvbuf_vals(&s, ak, idx);
			udpc_seria_add_str(&sctx, akfld, sizeof(akfld)-1);
			udpc_seria_add_str(&sctx, s, len);
		}
		/* serialise the emails */
		for (mvbsize_t idx = 0; idx < mvbuf_nvals(em); idx++) {
			const char *s;
			mvbsize_t len = mvbuf_vals(&s, em, idx);
			udpc_seria_add_str(&sctx, emfld, sizeof(emfld)-1);
			udpc_seria_add_str(&sctx, s, len);
		}
	}
	/* chop chop, off we go */
	send_pkt(&sctx, j);
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
