/*** dso-xdr-instr-file.c -- instruments in XDR notation file I/O
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-nifty.h"
#include "unserding-ctx.h"
#include "unserding-private.h"

#include <pfack/instruments.h>
#include "catalogue.h"
#include "xdr-instr-private.h"

/**
 * Service 4218:
 * Obtain instrument definitions from a file.
 * sig: 4218(string filename)
 * Returns nothing. */
#define UD_SVC_INSTR_FROM_FILE	0x4218

/**
 * Temporary service for instr fetching. */
#define UD_SVC_INSTR_TO_FILE	0x421c

/**
 * Fetch URN service. */
#define UD_SVC_FETCH_INSTR	0x421e

#if !defined xnew
# define xnew(_x)	malloc(sizeof(_x))
#endif	/* !xnew */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

#define q32_t	quantity32_t


/* guts */
static void
__add_from_file(const char *fname)
{
	XDR hdl;
	FILE *f;

	UD_DEBUG("getting XDR encoded instrument from %s ...", fname);
	if ((f = fopen(fname, "r")) == NULL) {
		UD_DBGCONT("failed\n");
		return;
	}

#define CAT	((struct cat_s*)instrs)
	pthread_mutex_lock(&CAT->mtx);

	xdrstdio_create(&hdl, f, XDR_DECODE);
	while (true) {
		struct instr_s i;

		init_instr(&i);
		if (!(xdr_instr_s(&hdl, &i))) {
			break;
		}
		(void)cat_bang_instr_nolck(instrs, &i);
	}
	xdr_destroy(&hdl);
	pthread_mutex_unlock(&CAT->mtx);
#undef CAT
	fclose(f);
	UD_DBGCONT("done\n");
	return;
}

static void
__dump_to_file(const char *fname)
{
	XDR hdl;
	FILE *f;

	UD_DEBUG("dumping into %s ...", fname);
	if ((f = fopen(fname, "w")) == NULL) {
		UD_DBGCONT("failed\n");
		return;
	}

/* fuck ugly, mutex'd iterators are a pita */
#define CAT	((struct cat_s*)instrs)
	pthread_mutex_lock(&CAT->mtx);

	xdrstdio_create(&hdl, f, XDR_ENCODE);
	for (index_t i = 0; i < CAT->ninstrs; i++) {
		instr_t instr = &((instr_t)CAT->instrs)[i];
		if (!xdr_instr_s(&hdl, instr)) {
			UD_DBGCONT("uhoh ...");
			break;
		}
	}
	xdr_destroy(&hdl);
	pthread_mutex_unlock(&CAT->mtx);
#undef CAT
	fclose(f);
	UD_DBGCONT("done\n");
}


/* gonna be a more generic fetch service one day */
static void
instr_add_from_file_svc(job_t j)
{
	/* our serialiser */
	struct udpc_seria_s sctx;
	char fname[256];

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	udpc_seria_des_str_into(fname, sizeof(fname), &sctx);
	__add_from_file(fname);
	return;
}

static void
instr_dump_to_file_svc(job_t j)
{
/* i think this has to disappear, file-backing should be opaque */
	struct udpc_seria_s sctx;
	char fname[256];

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PAYLLEN(j->blen));
	udpc_seria_des_str_into(fname, sizeof(fname), &sctx);
	__dump_to_file(fname);
	return;
}


static char xdrfname[256] = {'\0'};

void
fetch_instr_file(void)
{
	if (xdrfname[0] != '\0') {
		__add_from_file(xdrfname);
	}
	return;
}


/* initialiser */
void
dso_xdr_instr_file_LTX_init(void *clo)
{
	void *spec = udctx_get_setting(clo);
	const char *file = NULL;
	ud_ctx_t ctx = clo;

	UD_DEBUG("mod/xdr-instr-file: loading ...");
	udcfg_tbl_lookup_s(&file, ctx, spec, "file");
	if (file == NULL) {
		UD_DBGCONT("failed, no `file' specification found\n");
	}
	strncpy(xdrfname, file, sizeof(xdrfname));
	UD_DBGCONT("done\n");

	ud_set_service(UD_SVC_INSTR_FROM_FILE, instr_add_from_file_svc, NULL);
	ud_set_service(UD_SVC_INSTR_TO_FILE, instr_dump_to_file_svc, NULL);
	return;
}

void
dso_xdr_instr_file_LTX_deinit(void *UNUSED(clo))
{
	xdrfname[0] = '\0';
	ud_set_service(UD_SVC_INSTR_TO_FILE, NULL, NULL);
	ud_set_service(UD_SVC_INSTR_FROM_FILE, NULL, NULL);
	return;
}

/* dso-xdr-instr-file.c ends here */
