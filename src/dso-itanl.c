/*** dso-itanl.c -- itan lookup service
 *
 * Copyright (C) 2010 Sebastian Freundt
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
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#define UNSERSRV
#include "unserding.h"
#include "unserding-ctx.h"
#include "unserding-nifty.h"
#include "unserding-private.h"
#include "svc-itanl.h"
#include "seria-proto-glue.h"

#if !defined ESUCCESS
# define ESUCCESS	0
#endif	/* !ESUCCESS */

/* size hereby has to match the block cipher we've chosen */
typedef char itan_t[8];
/* very simplistic itan list, indexed naturally */
typedef itan_t tlst_t[120];


/* a simple and efficient cipher */
#include "btea.c"


static tlst_t gtlst;
static uint32_t key[4];

static bool
obtain(itan_t tgt, uint16_t idx)
{
	/* fetch the guy */
	memcpy(tgt, gtlst[idx - 1], sizeof(itan_t));
	/* decipher */
	btea_dec((void*)tgt, sizeof(itan_t) / sizeof(uint32_t), key);
	return tgt[7] == (char)idx;
}

static void
deposit(itan_t tgt, size_t tsz, uint16_t idx)
{
	/* cock off early if this isnt a tan */
	if (tsz > 8) {
		return;
	}
	/* blank the last two bytes */
	tgt[6] = '\0';
	tgt[7] = (char)idx;
	/* and encrypt it */
	btea_enc((void*)tgt, sizeof(itan_t) / sizeof(uint32_t), key);
	/* and store */
	memcpy(gtlst[idx - 1], tgt, sizeof(itan_t));
	return;
}

static void
itanl(job_t j)
{
	uint16_t itanno;
	struct udpc_seria_s sctx[1];
	itan_t tan;
	size_t tsz;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	itanno = udpc_seria_des_ui16(sctx);
	if ((tsz = udpc_seria_des_str_into(tan, sizeof(tan), sctx)) > 0) {
		/* setter mode */
		UD_DEBUG("0x%04x (UD_SVC_ITANL): setting itan %hu to %s\n",
			 UD_SVC_ITANL, itanno, tan);
		deposit(tan, tsz, itanno);
		return;
	}
	/* otherwise */
	UD_DEBUG("0x%04x (UD_SVC_ITANL): looking up itan %hu\n",
		 UD_SVC_ITANL, itanno);

	/* get the tan */
	if (obtain(tan, itanno)) {
		/* prepare the reply packet */
		clear_pkt(sctx, j);
		udpc_seria_add_str(sctx, tan, /* true size is */6);
		send_pkt(sctx, j);
	}
	return;
}


static void
setup_key(void *tgt, size_t len)
{
	/* just utilise /dev/urandom, shall we? */
	int fd = open("/dev/urandom", O_RDONLY);
	read(fd, tgt, len);
	close(fd);
	return;
}

static int
restore_from_file(void *UNUSED(clo), const char *fn)
{
	int fd, res = ESUCCESS;

	if ((fd = open(fn, O_RDONLY)) < 0) {
		return fd;
	}
	/* just read the container, encrypted as it is */
	res = read(fd, gtlst, sizeof(gtlst)) > 0;
	close(fd);
	return res;
}

static void
dump_to_file(void *UNUSED(clo), const char *fn)
{
	int fd;

	UD_DEBUG("dumping into %s ...", fn);
	if ((fd = open(fn, O_CREAT | O_TRUNC | O_RDWR), 0600) < 0) {
		UD_DBGCONT("failed\n");
		return;
	}
	/* just dump the container, encrypted as it is */
	write(fd, gtlst, sizeof(gtlst));

	close(fd);
	UD_DBGCONT("done\n");
	return;
}


static const char fname[] = "/home/freundt/.unserding/itans";

void
dso_itanl_LTX_init(void *clo)
{
	/* itan lookup service */
	UD_DEBUG("mod/itanl: loading ...");
	ud_set_service(UD_SVC_ITANL, itanl, NULL);
	setup_key(key, sizeof(key));
	if (restore_from_file(clo, fname)) {
		UD_DBGCONT("failed\n");
	}
	UD_DBGCONT("done\n");
	return;
}

void
dso_itanl_LTX_deinit(void *clo)
{
	ud_set_service(UD_SVC_ITANL, NULL, NULL);
	dump_to_file(clo, fname);
	return;
}

/* dso-itanl.c ends here */
