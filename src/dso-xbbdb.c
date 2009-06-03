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
#include "module.h"
#include "unserding.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include <sys/param.h>

#include "protocore.h"

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <libxml/xpath.h>

#define xnew(_x)	malloc(sizeof(_x))

typedef size_t index_t;

typedef xmlDocPtr bbdb_t;
typedef xmlNodePtr entry_t;

static bbdb_t recs = NULL;


static void
parse_fd(int fd)
{
	xmlTextReaderPtr rd;
	entry_t root;

#if !defined XML_PARSE_COMPACT
# define XML_PARSE_COMPACT	0
#endif	/* !XML_PARSE_COMPACT */
#if 0
	rd = xmlReaderForMemory(buf, buflen, NULL, NULL, XML_PARSE_COMPACT);
#else
	rd = xmlReaderForFd(fd, NULL, NULL, XML_PARSE_COMPACT);
#endif

	if (rd == NULL) {
		return;
	}
	if (xmlTextReaderRead(rd) < 1) {
		//("No nodes in that XML stream\n");
		recs = NULL;
		return;
	}
	if ((root = xmlTextReaderExpand(rd)) == NULL) {
		//("No root node?! Wtf?\n");
		recs = NULL;
		goto out;
	}

	recs = xmlTextReaderCurrentDoc(rd);
out:
	xmlFreeTextReader(rd);
	return;
}

static int
read_bbdb(void)
{
	const char bbdbfn[] = "/.bbdb.xml";
	char full[MAXPATHLEN], *tmp;
	int bbdbp;

	/* construct the name */
	tmp = stpcpy(full, getenv("HOME"));
	strncpy(tmp, bbdbfn, sizeof(bbdbfn));

	if ((bbdbp = open(full, 0)) < 0) {
		return 1;
	}

	/* read the file, line by line */
	parse_fd(bbdbp);
	close(bbdbp);
	return 0;
}

#if 0
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
#endif

static entry_t
find_entry(const char *str, size_t len)
{
#if 0
	/* traverse our entries */
	for (bbdb_rec_t r = recs; r; r = r->next) {
		if (boyer_moore(r->entry, r->enlen, str, len) != NULL) {
			return r;
		}
	}
#endif
	return recs->children;
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
	{
		const char *s = (const char*)xmlNodeGetContent(rec->children);
		size_t sl = strlen(s);
		udpc_seria_add_str(&sctx, s, sl);
	}

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
