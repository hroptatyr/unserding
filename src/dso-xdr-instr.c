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
#include <pfack/tick.h>

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
 * sig: 4220(si32 ts, bitset32 types, (si32 secu, si32 fund, si32 exch)+)
 *   As a wildcard for all funds or all exchanges 0x00000000 can be used. */
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
static cat_t instrs;


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

static inline void
hrclock_print(void)
{
	struct timespec tsp;
	clock_gettime(CLOCK_REALTIME, &tsp);
	fprintf(stderr, "%lu.%09u", tsp.tv_sec, (unsigned int)tsp.tv_nsec);
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
prep_pkt(udpc_seria_t sctx, job_t rplj, job_t srcj)
{
	memcpy(rplj, srcj, sizeof(*rplj));
	memset(UDPC_PAYLOAD(rplj->buf), 0, UDPC_PLLEN);
	udpc_make_rpl_pkt(JOB_PACKET(rplj));
	udpc_seria_init(sctx, UDPC_PAYLOAD(rplj->buf), UDPC_PLLEN);
	return;
}

static inline void
send_pkt(udpc_seria_t sctx, job_t j)
{
	j->blen = UDPC_HDRLEN + udpc_seria_msglen(sctx);
	send_cl(j);
	return;
}

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


/* tick services */
/* tickable instruments, including currency and pot */
typedef struct sparse_l1tick_s *sparse_l1tick_t;

struct secu_s {
	gaid_t instr;
	gaid_t curr;
	gaid_t pot;
};

struct sparse_l1tick_s {
	struct secu_s secu;
	struct l1tick_s tick;
};

static void
__seria_sparse_l1tick(udpc_seria_t sctx, sparse_l1tick_t t)
{
	udpc_seria_add_ui32(sctx, (int32_t)t->secu.instr);
	udpc_seria_add_ui32(sctx, (int32_t)t->secu.curr);
	udpc_seria_add_ui32(sctx, (int32_t)t->secu.pot);
	udpc_seria_add_ui32(sctx, (uint32_t)t->tick.ts);
	udpc_seria_add_ui32(sctx, (uint32_t)t->tick.nsec);
	udpc_seria_add_byte(sctx, (uint8_t)t->tick.tt);
	udpc_seria_add_ui32(sctx, (uint32_t)t->tick.value);
	return;
}

static void
instr_tick_svc(job_t j)
{
	struct udpc_seria_s sctx;
	struct udpc_seria_s rplsctx;
	struct job_s rplj;
	/* in args */
	int32_t ts, ticks;
	/* allow to filter for 64 instruments at once */
	struct secu_s filt[64];
	unsigned int nfilt = 0;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* just read si32s */
	ts = udpc_seria_des_si32(&sctx);
	ticks = udpc_seria_des_si32(&sctx);

	/* triples of instrument identifiers */
	do {
		filt[nfilt].instr = udpc_seria_des_si32(&sctx);
		filt[nfilt].curr = udpc_seria_des_si32(&sctx);
		filt[nfilt].pot = udpc_seria_des_si32(&sctx);
	} while (filt[nfilt].instr != 0 && ++nfilt < countof(filt));

	UD_DEBUG("0x4220: ts:%d filtered for %u instrs\n", ts, nfilt);
	/* prepare the reply packet ... */
	prep_pkt(&rplsctx, &rplj, j);
	/* here's what we'd do:
	 * in <- find_instr_by_gaid()
	 * resv[] <- find_tick_resources_by_fund_and_exch()
	 * for i in resv { tick[i][] <- find_tick_by_timestamp() }
	 * for i,j in resv,tick { seria(tick[i][j]) }
	 *
	 * for now however we just send of a dummy */
	for (int more = 1; more; ) {
		struct sparse_l1tick_s sl1t = {
			.secu.instr = filt[0].instr,
			.secu.curr = 73380,
			.secu.pot = 4,
			.tick.tt = PFTT_EOD,
			.tick.ts = ts,
			.tick.nsec = 0,
			.tick.value = 10000,
		};
		__seria_sparse_l1tick(&rplsctx, &sl1t);
		more = 0;
	}
	/* send what we've got */
	send_pkt(&rplsctx, &rplj);
	return;
}


#include <ev.h>

static ev_idle __attribute__((aligned(16))) __widle;

#if 0
/* ideally this is a separate snippet that reads the stuff from a
 * database and bangs it into unserding via network */
static void
add_trivial(void)
{
	struct instr_s i;

	make_tcnxxx_into(&i, 73380, PFACK_4217_EUR_IDX);
	copyadd_instr(&i);

	make_tcnxxx_into(&i, 73381, PFACK_4217_USD_IDX);
	copyadd_instr(&i);

	make_ffcpnx_into(&i, 5384697780, PFACK_4217_EUR_IDX, PFACK_4217_USD_IDX);
	copyadd_instr(&i);
	return;
}
#else
static void
add_trivial(void)
{
	return;
}
#endif

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

	add_trivial();

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
init(void *clo)
{
	ud_ctx_t ctx = clo;
	ev_idle *widle = &__widle;

	UD_DEBUG("mod/asn1-instr: loading ...");
	/* create the catalogue */
	instrs = make_cat();
	/* lodging our bbdb search service */
	ud_set_service(0x4216, instr_add_svc, NULL);
	ud_set_service(UD_SVC_INSTR_FROM_FILE, instr_add_from_file_svc, NULL);
	ud_set_service(UD_SVC_INSTR_BY_ATTR, instr_dump_svc, instr_add_svc);
	ud_set_service(0x421c, instr_dump_to_file_svc, NULL);
	/* tick service */
	ud_set_service(UD_SVC_TICK_BY_TS, instr_tick_svc, NULL);
	UD_DBGCONT("done\n");

	UD_DEBUG("deploying idle bomb ...");
	ev_idle_init(widle, deferred_dl);
	ev_idle_start(ctx->mainloop, widle);
	UD_DBGCONT("done\n");
	return;
}

/* dso-xdr-instr.c */
