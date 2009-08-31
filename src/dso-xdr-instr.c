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

/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"

#include <pfack/instruments.h>
#include "catalogue.h"
#include "xdr-instr-seria.h"
#include "xdr-instr-private.h"

/**
 * Service 4218:
 * Obtain instrument definitions from a file.
 * sig: 4218(string filename)
 * Returns nothing. */
#define UD_SVC_INSTR_FROM_FILE	0x4218

/**
 * Service 421a:
 * Get instrument definitions.
 * sig: 421a((si32 gaid)*)
 *   Returns the instruments whose ids match the given ones.  In the
 *   special case that no id is given, all instruments are dumped.
 **/
#define UD_SVC_INSTR_BY_ATTR	0x421a

/**
 * Service 4220:
 * Find ticks of one time stamp over instruments (market snapshot).
 * This service can be used to get a succinct set of ticks, usually the
 * last ticks before a given time stamp, for several instruments.
 * The ticks to return can be specified in a bitset.
 *
 * sig: 4220(ui32 ts, ui32 types, (ui32 secu, ui32 fund, ui32 exch)+)
 *   As a wildcard for all funds or all exchanges 0x00000000 can be used.
 *
 * The TYPES parameter is a bitset made up of PFTB_* values as specified
 * in pfack/tick.h */
#define UD_SVC_TICK_BY_TS	0x4220

/**
 * Service 4222:
 * Find ticks of one instrument over time (time series).
 * This service can be used if the focus is one particular instrument
 * (or one instrument at different exchanges) and the ticks to be returned
 * are numerous.
 * sig: 4222()
 **/
#define UD_SVC_TICK_BY_INSTR	0x4222

#if !defined xnew
# define xnew(_x)	malloc(sizeof(_x))
#endif	/* !xnew */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined ALGN16
# define ALGN16(_x)	__attribute__((aligned(16))) _x
#endif	/* !ALGN16 */

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
merge_instr(instr_t tgt, instr_t src)
{
	return true;
}

static void
copyadd_instr(instr_t i)
{
	instr_t resi;

	if ((resi = find_instr(i)) != NULL) {
		UD_DBGCONT("found him already ... merging ...");
		//abort();
		merge_instr(resi, i);
	} else {
		cat_bang_instr(instrs, i);
	}
	return;
}

#if 0
static ssize_t
read_file(char *restrict buf, size_t bufsz, const char *fname)
{
	int fd;
	ssize_t nrd;

	if ((fd = open(fname, O_RDONLY)) < 0) {
		return -1;
	}
	nrd = read(fd, buf, bufsz);
	close(fd);
	return nrd;
}

static ssize_t
write_file(char *restrict buf, size_t bufsz, const char *fname)
{
	int fd;
	ssize_t nrd;

	if ((fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
		return -1;
	}
	nrd = write(fd, buf, bufsz);
	close(fd);
	return nrd;
}
#endif


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
instr_add_from_file_svc(job_t j)
{
	/* our serialiser */
	struct udpc_seria_s sctx;
	const char *sstrp;
	size_t ssz;
	XDR hdl;
	FILE *f;

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	ssz = udpc_seria_des_str(&sctx, &sstrp);

	if (ssz == 0) {
		UD_DEBUG("Usage: 4218 \"/path/to/file.xdr\"\n");
		return;
	}

	UD_DEBUG("getting XDR encoded instrument from %s\n", sstrp);

	if ((f = fopen(sstrp, "r")) == NULL) {
		UD_DEBUG("no such file or directory\n");
		return;
	}

	hrclock_print();
	fprintf(stderr, " start\n");

	xdrstdio_create(&hdl, f, XDR_DECODE);
	while (true) {
		struct instr_s i;

		init_instr(&i);
		if (!(xdr_instr_s(&hdl, &i))) {
			break;
		}
		(void)cat_bang_instr(instrs, &i);
	}
	xdr_destroy(&hdl);

	fclose(f);

	hrclock_print();
	fprintf(stderr, " stop\n");
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
	pthread_mutex_lock(&CAT->mtx);
	UD_DEBUG("dumping %u instruments ...", (unsigned int)CAT->ninstrs);

	do {
		char *enc = &j->buf[UDPC_HDRLEN];
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

	UD_DEBUG("dumping %p ...", in);
	/* serialise what we've got */
	__seria_instr(sctx, in);
	UD_DBGCONT("done\n");
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

static void
instr_dump_to_file_svc(job_t j)
{
	return;
}


#include <ev.h>

static ev_idle __attribute__((aligned(16))) __widle;

static void
deferred_dl(EV_P_ ev_idle *w, int revents)
{
	struct job_s j;
	ud_packet_t __pkt = {.pbuf = j.buf};

	UD_DEBUG("downloading instrs ...");
	udpc_make_pkt(__pkt, 0, 0, UD_SVC_INSTR_BY_ATTR);
	j.blen = UDPC_HDRLEN;
	send_m46(&j);
	UD_DBGCONT("done\n");

	ev_idle_stop(EV_A_ w);
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

void
dso_xdr_instr_LTX_init(void *clo)
{
	ud_ctx_t ctx = clo;
	ev_idle *widle = &__widle;

	UD_DEBUG("mod/xdr-instr: loading ...");
	/* create the catalogue */
	instrs = make_cat();
	/* lodging our bbdb search service */
	ud_set_service(0x4216, instr_add_svc, NULL);
	ud_set_service(UD_SVC_INSTR_FROM_FILE, instr_add_from_file_svc, NULL);
	ud_set_service(UD_SVC_INSTR_BY_ATTR, instr_dump_svc, instr_add_svc);
	ud_set_service(0x421c, instr_dump_to_file_svc, NULL);
	UD_DBGCONT("done\n");

	UD_DEBUG("deploying idle bomb ...");
	ev_idle_init(widle, deferred_dl);
	ev_idle_start(ctx->mainloop, widle);
	UD_DBGCONT("done\n");

	/* load the ticks service */
	dso_xdr_instr_ticks_LTX_init(clo);
	return;
}

/* dso-xdr-instr.c */
