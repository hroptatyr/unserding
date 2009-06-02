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
#endif	/* HAVE_CONFIG_H */
#include <stdio.h>
#include <string.h>
#include "module.h"
#include "unserding.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include <sys/param.h>

#include "protocore.h"

#define xnew(_x)	malloc(sizeof(_x))

typedef size_t index_t;

struct bbdb_ol_s {
	/* offset+length */
	uint16_t off;
	uint16_t len;
};

typedef struct bbdb_rec_s *bbdb_rec_t;
struct bbdb_rec_s {
	bbdb_rec_t next;
	char *entry;
	/* length of entry */
	size_t enlen;
	/* offsets into the entry */
	struct bbdb_ol_s gname;
	struct bbdb_ol_s sname;
	struct bbdb_ol_s emails;
};

static bbdb_rec_t recs = NULL;


static void
parse_common_fields(bbdb_rec_t r)
{
	index_t i;

	if (r->entry[0] != '[' || r->entry[1] != '"') {
		/* malformed entry */
		return;
	}

	/* fetch the given name */
	r->gname.off = 2;
	for (i = 2; i < 0xfffe; i++) {
		if (r->entry[i] == '"' && r->entry[i-1] != '\\') {
			/* found the end */
			r->gname.len = (uint16_t)i - r->gname.off;
			break;
		}
	}

	if (r->entry[++i] != ' ' || r->entry[++i] != '"') {
		/* malformed entry */
		return;
	}

	/* fetch the surname */
	r->sname.off = (uint16_t)++i;
	for (; i < 0xfffe; i++) {
		if (r->entry[i] == '"' && r->entry[i-1] != '\\') {
			/* found the end */
			r->sname.len = (uint16_t)i - r->sname.off;
			break;
		}
	}
	return;
}

static int
read_bbdb(void)
{
	const char bbdbfn[] = "/.bbdb";
	char full[MAXPATHLEN], *tmp;
	FILE *bbdbp;
	size_t bsz = 4096;

	/* construct the name */
	tmp = stpcpy(full, getenv("HOME"));
	strncpy(tmp, bbdbfn, sizeof(bbdbfn));

	if ((bbdbp = fopen(full, "r")) == NULL) {
		return 1;
	}

	/* read the file, line by line */
	tmp = malloc(bsz);
	for (ssize_t n; (n = getline(&tmp, &bsz, bbdbp)) > 0;) {
		bbdb_rec_t r = xnew(*r);
		r->next = recs;
		r->entry = malloc(n);
		memcpy(r->entry, tmp, n - 1);
		r->entry[n - 1] = '\0';
		r->enlen = n-1;
		/* parse the fields */
		parse_common_fields(r);
		/* cons him to the front of our recs */
		recs = r;
	}
	free(tmp);
	fclose(bbdbp);
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

static bbdb_rec_t
find_entry(const char *str, size_t len)
{
	/* traverse our entries */
	for (bbdb_rec_t r = recs; r; r = r->next) {
		if (boyer_moore(r->entry, r->enlen, str, len) != NULL) {
			return r;
		}
	}
	return NULL;
}


/* #'bbdb-search */
static void __attribute__((unused))
bbdb_search(job_t j)
{
	struct udpc_seria_s sctx;
	size_t ssz;
	const char *sstr;
	bbdb_rec_t rec;

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	ssz = udpc_seria_des_str(&sctx, &sstr);
	UD_DEBUG("mod/bbdb: <- search \"%s\"\n", sstr);

	if (ssz == 0) {
		return;
	}

	if ((rec = find_entry(sstr, ssz)) == NULL) {
		return;
	}

	for (size_t k = 0, done, togo = rec->enlen; togo > 0;
	     k += done, togo -= done) {
		udpc_make_rpl_pkt(JOB_PACKET(j));
		udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);

		done = udpc_seria_add_fragstr(&sctx, &rec->entry[k], togo);

		j->blen = UDPC_HDRLEN + udpc_seria_msglen(&sctx);
		send_cl(j);
	}
	return;
}

static void
bbdb_search_tagged(job_t j)
{
	struct udpc_seria_s sctx;
	size_t ssz;
	const char *sstr;
	bbdb_rec_t rec;
	static const char gnfld[] = "gname";
	static const char snfld[] = "sname";

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	ssz = udpc_seria_des_str(&sctx, &sstr);
	UD_DEBUG("mod/bbdb: <- search \"%s\"\n", sstr);

	if (ssz == 0) {
		return;
	}

	if ((rec = find_entry(sstr, ssz)) == NULL) {
		return;
	}

	udpc_make_rpl_pkt(JOB_PACKET(j));
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	if (rec->gname.len > 0) {
		udpc_seria_add_str(&sctx, gnfld, sizeof(gnfld)-1);
		udpc_seria_add_str(
			&sctx, &rec->entry[rec->gname.off], rec->gname.len);
	}

	if (rec->sname.len > 0) {
		udpc_seria_add_str(&sctx, snfld, sizeof(snfld)-1);
		udpc_seria_add_str(
			&sctx, &rec->entry[rec->sname.off], rec->sname.len);
	}

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
		ud_set_service(0xbbda, bbdb_search_tagged, NULL);
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
		ud_set_service(0xbbda, bbdb_search_tagged, NULL);
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
