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

#include <stdio.h>
#include "module.h"
#include "unserding.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include <sys/param.h>
#include <string.h>

#include "protocore.h"

#define xnew(_x)	malloc(sizeof(_x))

typedef struct bbdb_rec_s *bbdb_rec_t;
struct bbdb_rec_s {
	bbdb_rec_t next;
	char *entry;
	/* offsets into the entry */
	size_t gname;
	size_t sname;
	size_t emails;
};

static bbdb_rec_t recs = NULL;


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
		recs = r;
	}
	free(tmp);
	fclose(bbdbp);
	return 0;
}


/* #'bbdb-search */
static void
bbdb_search(job_t j)
{
	struct udpc_seria_s sctx;
	size_t mic = 252;
	size_t ssz;
	const char *sstr;

	udpc_seria_init(&sctx, &j->buf[UDPC_SIG_OFFSET], JOB_BUF_SIZE);
	ssz = udpc_seria_des_str(&sctx, &sstr);
	UD_DEBUG("mod/bbdb: <- search \"%s\"\n", sstr);

	for (size_t i = strlen(recs->entry), k = 0; k < i; k += 5*mic) {
		udpc_make_rpl_pkt(JOB_PACKET(j));
		udpc_seria_init(&sctx, &j->buf[UDPC_SIG_OFFSET], JOB_BUF_SIZE);

		udpc_seria_add_str(&sctx, &recs->entry[k+0*mic], mic);
		udpc_seria_add_str(&sctx, &recs->entry[k+1*mic], mic);
		udpc_seria_add_str(&sctx, &recs->entry[k+2*mic], mic);
		udpc_seria_add_str(&sctx, &recs->entry[k+3*mic], mic);
		udpc_seria_add_str(&sctx, &recs->entry[k+4*mic], mic);

		j->blen = UDPC_SIG_OFFSET + udpc_seria_msglen(&sctx);
		send_cl(j);
	}
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
	/* lodging our bbdb search service */
	ud_set_service(0xbbda, bbdb_search, NULL);
	UD_DBGCONT("done\n");
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
