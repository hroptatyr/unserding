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

/* very simplistic itan list, indexed naturally */
typedef char itan_t[6];
typedef itan_t tlst_t[120];

static tlst_t gtlst;

static void
itanl(job_t j)
{
	uint16_t itanno;
	struct udpc_seria_s sctx[1];

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	itanno = udpc_seria_des_ui16(sctx);
	UD_DEBUG("0x%04x (UD_SVC_ITANL): looking up itan %hu\n",
		 UD_SVC_ITANL, itanno);

	/* prepare the reply packet */
	clear_pkt(sctx, j);
	udpc_seria_add_str(sctx, gtlst[itanno-1], sizeof(gtlst[itanno-1]));
	send_pkt(sctx, j);
	return;
}


static int
read_itan_lists(void *UNUSED(clo))
{
	int fd, res = ESUCCESS;

	if ((fd = open("/tmp/l3", O_RDONLY)) < 0) {
		return fd;
	}
	for (unsigned int i = 0; i < countof(gtlst); i++) {
		char dummy[1];

		if (read(fd, gtlst[i], sizeof(gtlst[i])) <= 0) {
			res = -1;
		}
		/* there should be a CR there */
		if (read(fd, dummy, sizeof(dummy)) < 1 || dummy[0] != '\n') {
			res = -1;
		}
	}
	close(fd);
	return res;
}


void
dso_itanl_LTX_init(void *clo)
{
	/* itan lookup service */
	UD_DEBUG("mod/itanl: loading ...");
	ud_set_service(UD_SVC_ITANL, itanl, NULL);
	if (read_itan_lists(clo)) {
		UD_DBGCONT("failed\n");
	}
	UD_DBGCONT("done\n");
	return;
}

void
dso_itanl_LTX_deinit(void *UNUSED(clo))
{
	ud_set_service(UD_SVC_ITANL, NULL, NULL);
	return;
}

/* dso-itanl.c ends here */
