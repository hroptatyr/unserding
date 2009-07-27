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

struct instr_cons_s {
	instr_cons_t next;
	void *instr;
};


static instr_cons_t instruments;

static char xer_buf[4096];

#define XER_PATH	"/tmp/instr.xer"

static void
instr_add(job_t j)
{
	UD_DEBUG("adding instrument\n");
	return;
}

static void
instr_add_xer(job_t j)
{
	int fd;
	ssize_t nrd;
	asn_codec_ctx_t *opt_codec_ctx = NULL;
	void *s = NULL;
	asn_dec_rval_t rval;
	asn_TYPE_descriptor_t *pdu = &asn_DEF_Instrument;

	UD_DEBUG("getting shit from /tmp/instr.xer ...");
	if ((fd = open(XER_PATH, O_RDONLY)) < 0) {
		UD_DBGCONT("failed\n");
		return;
	}
	nrd = read(fd, xer_buf, sizeof(xer_buf));
	close(fd);

	/* decode */
	rval = xer_decode(opt_codec_ctx, pdu, &s, xer_buf, nrd);
	if (rval.code == RC_OK) {
		instr_cons_t ic = xnew(*ic);
		ic->instr = s;
		ic->next = instruments;
		instruments = ic;
		UD_DBGCONT("success\n");
	} else {
		UD_DBGCONT("failed %d\n", rval.code);
	}
	return;
}


void
init(void *clo)
{
	UD_DEBUG("mod/asn1-inestr: loading ...");
	/* lodging our bbdb search service */
	ud_set_service(0x4216, instr_add, NULL);
	ud_set_service(0x4218, instr_add_xer, NULL);
	UD_DBGCONT("done\n");
	return;
}


/* dso-asn1-instr.c */
