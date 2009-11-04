/*** dso-xdr-instr.c -- instruments in XDR notation
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

/* our local catalogue */
cat_t instrs;


/* aux */
/**
 * Return the instrument in the catalogue that matches I, or NULL
 * if no such instrument exists. */
static instr_t
find_instr(instr_t i)
{
	/* strategy is
	 * 1. find_by_gaid
	 * 2. find_by_isin_cfi_opol
	 * 3. find_by_name */
	instr_t resi;
	ident_t ii = instr_ident(i);

	if ((resi = find_instr_by_gaid(instrs, ident_gaid(ii))) != NULL) {
		return resi;
#if 0
/* unused atm */
	} else if ((resi = find_instr_by_isin_cfi_opol(ii)) != NULL) {
		return resi;
#endif
#if 0
/* names are currently not unique */
	} else if ((resi = find_instr_by_name(ii)) != NULL) {
		return resi;
#endif
	} else {
		return NULL;
	}
}

/**
 * Merge SRC into TGT if of the same kind, return TRUE if no
 * conflicts occurred. */
static bool
merge_instr(instr_t UNUSED(tgt), instr_t UNUSED(src))
{
	return true;
}

static void
copyadd_instr(instr_t i)
{
	instr_t resi;

	if ((resi = find_instr(i)) != NULL) {
		UD_DBGCONT("found him already ... merging ...");
		merge_instr(resi, i);
	} else {
		i = cat_bang_instr(instrs, i);
	}
	return;
}


/* jobs */
static void
instr_add_svc(job_t j)
{
/* would it be wise to treat this like the from-file case? */
	/* our stuff */
	struct udpc_seria_s sctx;
	size_t len;
	const void *dec_buf = NULL;
	struct instr_s s;

	UD_DEBUG("adding instrument ...");

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	len = udpc_seria_des_xdr(&sctx, &dec_buf);

	len = deser_instrument_into(&s, dec_buf, len);
	if (len > 0) {
		copyadd_instr(&s);
		UD_DBGCONT("success\n");
	} else {
		UD_DBGCONT("failed\n");
	}
	return;
}

static void
instr_dump_all(job_t j)
{
	/* our stuff */
	size_t i = 0;
	XDR hdl;

/* fuck ugly, mutex'd iterators are a pita */
#define CAT	((struct cat_s*)instrs)
	UD_DEBUG("dumping %zu instrs ...", CAT->ninstrs);
	pthread_mutex_lock(&CAT->mtx);

	do {
		char *enc = UDPC_PAYLOAD(j->buf);
		size_t len;

		/* prepare the packet ... */
		udpc_make_rpl_pkt(JOB_PACKET(j));
		/* we just serialise him ourselves */
		enc[0] = UDPC_TYPE_XDR;
		enc[1] = enc[2] = 0;

		xdrmem_create(&hdl, enc + 3, UDPC_PLLEN - 3, XDR_ENCODE);
		for (; i < CAT->ninstrs; i++) {
			instr_t instr = &((instr_t)CAT->instrs)[i];
			if (!xdr_instr_s(&hdl, instr)) {
				break;
			}
		}
		/* clean up */
		len = xdr_getpos(&hdl);
		xdr_destroy(&hdl);
		/* put in the size */
		enc[1] = (uint8_t)(len >> 8);
		enc[2] = (uint8_t)(len & 0xff);
		/* ... and send him off */
		j->blen = UDPC_HDRLEN + 3 + len;
		send_cl(j);
	} while (i < CAT->ninstrs);
	pthread_mutex_unlock(&CAT->mtx);
#undef CAT
	UD_DBGCONT("done\n");
	return;
}

/* one-at-a-time dispatchers */
static inline void
__seria_instr(udpc_seria_t sctx, instr_t in)
{
/* IN is guaranteed to be non-NULL */
	uint16_t len = 0;
	uint16_t max_len = sctx->len - sctx->msgoff - XDR_HDR_LEN;
	char *buf = &sctx->msg[sctx->msgoff + XDR_HDR_LEN];

	/* tag him as xdr */
	sctx->msg[sctx->msgoff + 0] = UDPC_TYPE_XDR;
	if (LIKELY(in != NULL)) {
		len = (uint16_t)seria_instrument(buf, max_len, in);
	}

	sctx->msg[sctx->msgoff + 1] = (uint8_t)(len >> 8);
	sctx->msg[sctx->msgoff + 2] = (uint8_t)(len & 0xff);
	sctx->msgoff += XDR_HDR_LEN + len;
	return;
}

static void
instr_dump_gaid(udpc_seria_t sctx, gaid_t gaid)
{
	instr_t in = find_instr_by_gaid(instrs, gaid);

	/* serialise what we've got */
	__seria_instr(sctx, in);
	return;
}

static void
instr_dump_svc(job_t j)
{
	struct udpc_seria_s sctx;
	struct udpc_seria_s rplsctx;
	struct job_s rplj;
	size_t cnt = 0;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);

	/* prepare the reply packet ... */
	prep_pkt(&rplsctx, &rplj, j);

	do {
		switch (udpc_seria_tag(&sctx)) {
		case UDPC_TYPE_STR:
			/* find by name */
			break;
		case UDPC_TYPE_SI32: {
			/* find by gaid */
			int32_t id = udpc_seria_des_si32(&sctx);
			instr_dump_gaid(&rplsctx, id);
			break;
		}
		case UDPC_TYPE_UNK:
		default:
			if (!cnt) {
				instr_dump_all(j);
				return;
			}
			goto out;
		}
	} while (++cnt);
out:
	/* send what we've got */
	send_pkt(&rplsctx, &rplj);
	return;
}


#if 0
static void
deferred_dl(BLA *w, int revents)
{
#if 0
	struct job_s j;
	ud_packet_t __pkt = {.pbuf = j.buf};

	udpc_make_pkt(__pkt, 0, 0, UD_SVC_INSTR_BY_ATTR);
	j.blen = UDPC_HDRLEN;
	send_m46(&j);
#endif


#if 0
#define SOCK_DCCP 6      
#define IPPROTO_DCCP 33
#define SOL_DCCP 269    
#define MAX_DCCP_CONNECTION_BACK_LOG 5
	struct sockaddr_in salocal;
	struct sockaddr_in saremote;
	socklen_t saremote_len = sizeof(saremote);

	int s = socket(PF_INET, SOCK_DCCP, IPPROTO_DCCP);
	/* turn off bind address checking, and allow port numbers to be reused -
	 * otherwise the TIME_WAIT phenomenon will prevent binding to these
	 * address.port combinations for (2 * MSL) seconds. */
	int on = 1;
	int result;
	result = setsockopt(s, SOL_DCCP, SO_REUSEADDR, (const char *)&on, sizeof(on));
	result = setsockopt(s, SOL_DCCP, 2, (const char *)&on, sizeof(on));

	memset(&salocal, 0, sizeof(salocal));
	salocal.sin_family = AF_INET;
	salocal.sin_addr.s_addr = INADDR_ANY;
	salocal.sin_port = htons(12121);
	/* bind the socket to the local address and port. salocal is sockaddr
	 * of local IP and port */
	result = bind(s, (struct sockaddr *)&salocal, sizeof(salocal));
	UD_DEBUG("result %d\n", result);
	/* listen on that port for incoming connections */
	result = listen(s, MAX_DCCP_CONNECTION_BACK_LOG);
	UD_DEBUG("result %d\n", result);
	/* wait to accept a client connecting. When a client joins,
	 * mRemoteName and mRemoteLength are filled out by accept() */
	UD_DEBUG("accepting shite ...");
	int x = accept(s, (struct sockaddr *)&saremote, &saremote_len);
	UD_DBGCONT("done\n");
#endif	/* 0 */
	return;
}
#endif	/* big-0 */


/* (re)fetch instr svc */
static void
fetch_instr_svc(job_t UNUSED(j))
{
	UD_DEBUG("0x%04x (UD_SVC_FETCH_INSTR)\n", UD_SVC_FETCH_INSTR);
	ud_set_service(UD_SVC_FETCH_INSTR, NULL, NULL);
#if defined HAVE_MYSQL
	fetch_instr_mysql();
#endif	/* HAVE_MYSQL */
	fetch_instr_file();
	ud_set_service(UD_SVC_FETCH_INSTR, fetch_instr_svc, NULL);
	return;
}


/* config file mumbo jumbo */
static void*
cfgspec_get_source(ud_ctx_t ctx, ud_cfgset_t spec)
{
#define CFG_SOURCE	"source"
	return udcfg_tbl_lookup(ctx, spec, CFG_SOURCE);
}

typedef enum {
	CST_UNK,
	CST_MYSQL,
	CST_XDRFILE,
} cfgsrc_type_t;

static cfgsrc_type_t
cfgsrc_type(void *ctx, void *spec)
{
#define CFG_TYPE	"type"
	const char *type = NULL;

	if (spec == NULL) {
		UD_DEBUG("no source specified\n");
		return CST_UNK;
	}
	udcfg_tbl_lookup_s(&type, ctx, spec, CFG_TYPE);

	UD_DEBUG("type %s %p\n", type, spec);
	if (type == NULL) {
		return CST_UNK;
	} else if (memcmp(type, "mysql", 5) == 0) {
		return CST_MYSQL;
	} else if (memcmp(type, "xdrcube", 7) == 0) {
		return CST_XDRFILE;
	}
	return CST_UNK;
}

static void
load_instr_fetcher(void *clo, void *spec)
{
	void *src = cfgspec_get_source(clo, spec);

	/* prepare source settings to be passed along */
	udctx_set_setting(clo, src);
	
	/* find out about its type */
	switch (cfgsrc_type(clo, src)) {
	case CST_MYSQL:
#if defined HAVE_MYSQL
		/* fetch some instruments by sql */
		dso_xdr_instr_mysql_LTX_init(clo);
#endif	/* HAVE_MYSQL */
		break;

	case CST_XDRFILE:
		dso_xdr_instr_file_LTX_init(clo);
		break;

	case CST_UNK:
	default:
		/* do fuckall */
		break;
	}

	/* clean up */
	udctx_set_setting(clo, NULL);
	udcfg_tbl_free(clo, src);
	return;
}

static void
unload_instr_fetcher(void *UNUSED(clo))
{
#if defined HAVE_MYSQL
	dso_xdr_instr_mysql_LTX_deinit(clo);
#endif	/* HAVE_MYSQL */
	dso_xdr_instr_file_LTX_deinit(clo);
	return;
}


void
dso_xdr_instr_LTX_init(void *clo)
{
	ud_ctx_t ctx = clo;
	void *settings;

	UD_DEBUG("mod/xdr-instr: loading ...");
	/* create the catalogue */
	instrs = make_cat();
	/* lodging our bbdb search service */
	ud_set_service(0x4216, instr_add_svc, NULL);
	ud_set_service(UD_SVC_INSTR_BY_ATTR, instr_dump_svc, instr_add_svc);
	ud_set_service(UD_SVC_FETCH_INSTR, fetch_instr_svc, NULL);
	UD_DBGCONT("done\n");

	if ((settings = udctx_get_setting(ctx)) != NULL) {
		/* we are configured, load the instrs */
		load_instr_fetcher(clo, settings);
		/* be so kind as to unref the settings */
		udcfg_tbl_free(ctx, settings);
	}
	/* clean up */
	udctx_set_setting(ctx, NULL);

	/* kick off a fetch-INSTR job */
	wpool_enq(gwpool, (wpool_work_f)fetch_instr_svc, NULL, true);
	return;
}

void
dso_xdr_instr_LTX_deinit(void *clo)
{
	/* unload the fetchers, they possibly need the catalogue */
	unload_instr_fetcher(clo);
	/* unload the instrs now */
	free_cat(instrs);
	ud_set_service(UD_SVC_INSTR_BY_ATTR, NULL, NULL);
	ud_set_service(UD_SVC_FETCH_INSTR, NULL, NULL);
	return;
}

/* dso-xdr-instr.c */
