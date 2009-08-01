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

/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"

#include <pfack/instruments.h>

#define xnew(_x)	malloc(sizeof(_x))

/* VEEERY simple catalogue */
typedef struct instr_cons_s *instr_cons_t;

struct instr_cons_s {
	instr_cons_t next;
	void *instr;
	/* unsure about this one */
	void *resources[2];
};


static instr_cons_t instruments;

/* aux */
static inline bool
ident_gaid_equal_p(const_ident_t i1, const_ident_t i2)
{
	return i1->gaid == i2->gaid && i1->gaid != 0;
}

static inline bool
ident_name_equal_p(const_ident_t i1, const_ident_t i2)
{
	return memcmp(i1->name, i2->name, sizeof(i1->name));
}

static instr_t
find_instr_by_gaid(ident_t i)
{
	for (instr_cons_t ic = instruments; ic; ic = ic->next) {
		ident_t this_ident = instr_ident(ic->instr);
		if (ident_gaid_equal_p(this_ident, i)) {
			return ic->instr;
		}
	}
	return NULL;
}

static instr_t
find_instr_by_isin_cfi_opol(ident_t i)
{
	return NULL;
}

static instr_t
find_instr_by_name(ident_t i)
{
	for (instr_cons_t ic = instruments; ic; ic = ic->next) {
		ident_t this_ident = instr_ident(ic->instr);
		if (ident_name_equal_p(this_ident, i)) {
			return ic->instr;
		}
	}
	return NULL;
}

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

	if ((resi = find_instr_by_gaid(ii)) != NULL) {
		return resi;
	} else if ((resi = find_instr_by_isin_cfi_opol(ii)) != NULL) {
		return resi;
	} else if ((resi = find_instr_by_name(ii)) != NULL) {
		return resi;
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
add_instr(instr_t i)
{
#if 0
	instr_t resi;

	if ((resi = find_instr(i)) != NULL) {
		merge_instr(resi, i);
		ASN_STRUCT_FREE(asn_DEF_Instrument, i);
	} else {
		instr_cons_t ic = xnew(*ic);
		ic->instr = i;
		ic->next = instruments;
		instruments = ic;
	}
#endif
	return;
}

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


/* jobs */
static void
instr_add(job_t j)
{
	/* our stuff */
	struct udpc_seria_s sctx;
	size_t len;
	const void *ber_buf = NULL;
	instr_t s = NULL;

	UD_DEBUG("adding instrument ...");

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	len = udpc_seria_des_asn1(&sctx, &ber_buf);

#if 0
	rv = ber_decode(ctx, pdu, (void**)&s, ber_buf, len);
	if (rv.code == RC_OK) {
		add_instr(s);
		UD_DBGCONT("success\n");
	} else {
		UD_DBGCONT("failed %d\n", rv.code);
	}
#endif
	return;
}

#if 0
static void
instr_add_from_file(job_t j)
{
	/* our serialiser */
	ssize_t nrd;
	instr_t s = NULL;
	struct udpc_seria_s sctx;
	size_t ssz;
	const char *sstrp;
	char xer_buf[4096];

	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	ssz = udpc_seria_des_str(&sctx, &sstrp);

	if (ssz == 0) {
		UD_DEBUG("Usage: 4218 \"/path/to/file.xer\"\n");
		return;
	}

	UD_DEBUG("getting XER encoded instrument from %s ...", sstrp);
	if ((nrd = read_file(xer_buf, sizeof(xer_buf), sstrp)) < 0) {
		UD_DBGCONT("failed\n");
		return;
	}

	/* decode */
	rval = xer_decode(ctx, pdu, (void**)&s, xer_buf, nrd);
	if (rval.code == RC_OK) {
		add_instr(s);
		UD_DBGCONT("success\n");
	} else {
		UD_DBGCONT("failed %d\n", rval.code);
	}
	return;
}
#endif

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

static void
instr_dump(job_t j)
{
	/* our stuff */
	struct udpc_seria_s sctx;

	UD_DEBUG("dumping instruments ...");

	/* prepare the packet ... */
	prep_pkt(&sctx, j);
	for (instr_cons_t ic = instruments; ic; ic = ic->next) {
#if 0
		/* asn.1 stuff */
		asn_TYPE_descriptor_t *pdu = &asn_DEF_Instrument;
		asn_enc_rval_t rv;
		char der_buf[UDPC_PLLEN];
		instr_t i = ic->instr;

		rv = der_encode_to_buffer(pdu, i, der_buf, sizeof(der_buf));
		if (rv.encoded >= 0) {
			udpc_seria_add_asn1(&sctx, der_buf, rv.encoded);
		}
#endif
	}
	/* ... and send him off */
	send_pkt(&sctx, j);
	UD_DBGCONT("done\n");
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
	udpc_make_pkt(__pkt, 0, 0, 0x4220);
	j.blen = UDPC_HDRLEN;
	send_m46(&j);
	UD_DBGCONT("done\n");

	ev_idle_stop(EV_A_ w);
	return;
}

void
init(void *clo)
{
	ud_ctx_t ctx = clo;
	ev_idle *widle = &__widle;

	UD_DEBUG("mod/asn1-instr: loading ...");
	/* lodging our bbdb search service */
	ud_set_service(0x4216, instr_add, NULL);
#if 0
/* from file, later */
	ud_set_service(0x4218, instr_add_xer, NULL);
#endif
	ud_set_service(0x4220, instr_dump, instr_add);
	UD_DBGCONT("done\n");

	UD_DEBUG("deploying idle bomb ...");
	ev_idle_init(widle, deferred_dl);
	ev_idle_start(ctx->mainloop, widle);
	UD_DBGCONT("done\n");
	return;
}

/* dso-xdr-instr.c */
