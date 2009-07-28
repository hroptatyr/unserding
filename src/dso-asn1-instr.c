/*** dso-asn1-instr.c -- instruments in ASN notation
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

#include "Instrument.h"

#define xnew(_x)	malloc(sizeof(_x))

typedef struct instr_cons_s *instr_cons_t;
typedef struct Instrument *instr_t;

struct instr_cons_s {
	instr_cons_t next;
	void *instr;
};


static instr_cons_t instruments;

/* aux */
static void
add_instr(instr_t i)
{
	instr_cons_t ic = xnew(*ic);
	ic->instr = i;
	ic->next = instruments;
	instruments = ic;
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
	UD_DEBUG("adding instrument\n");
	return;
}

static void
instr_add_xer(job_t j)
{
	ssize_t nrd;
	asn_codec_ctx_t *opt_codec_ctx = NULL;
	void *s = NULL;
	asn_dec_rval_t rval;
	asn_TYPE_descriptor_t *pdu = &asn_DEF_Instrument;
	/* our serialiser */
	struct udpc_seria_s sctx;
	size_t ssz;
	const char *sstrp;
	static char xer_buf[4096];

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
	rval = xer_decode(opt_codec_ctx, pdu, &s, xer_buf, nrd);
	if (rval.code == RC_OK) {
		add_instr(s);
		UD_DBGCONT("success\n");
	} else {
		UD_DBGCONT("failed %d\n", rval.code);
	}
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

static void
instr_dump(job_t j)
{
	asn_enc_rval_t rv;
	struct udpc_seria_s __sctx, *sctx = &__sctx;
	size_t len;

	UD_DEBUG("dumping instruments\n");

	if (instruments == NULL) {
		return;
	}

	prep_pkt(sctx, j);
#if 0
	for (instr_cons_t ic = instruments; ic; ic = ic->next) {
	}
#endif
	sctx->msg[sctx->msgoff + 0] = UDPC_TYPE_ASN1;
	rv = der_encode_to_buffer(
		&asn_DEF_Instrument, instruments->instr,
		UDPC_PAYLOAD(j->buf) + 3, UDPC_PLLEN - 3);
	if (rv.encoded < 0) {
		return;
	}
	len = rv.encoded;
	sctx->msg[sctx->msgoff + 1] = (uint8_t)(len >> 8);
	sctx->msg[sctx->msgoff + 2] = (uint8_t)(len & 0xff);
	/* chop chop, off we go */
	send_pkt(sctx, j);
	return;
}


void
init(void *clo)
{
	UD_DEBUG("mod/asn1-inestr: loading ...");
	/* lodging our bbdb search service */
	ud_set_service(0x4216, instr_add, NULL);
	ud_set_service(0x4218, instr_add_xer, NULL);
	ud_set_service(0x4220, instr_dump, NULL);
	UD_DBGCONT("done\n");
	return;
}


/* dso-asn1-instr.c */
