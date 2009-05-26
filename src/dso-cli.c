/*** dso-cli.c -- command line interface module
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

#include <stdio.h>
#include "module.h"
#include "unserding.h"
#define UNSERSRV
#include "unserding-dbg.h"

#include "protocore.h"

static char hn[64];
static size_t hnlen;


/* the HY service */
static void
cli_hy(job_t j)
{
	UD_DEBUG("mod/cli: %s<- HY\n", hn);
	udpc_make_rpl_pkt(JOB_PACKET(j));
	j->buf[UDPC_SIG_OFFSET] = UDPC_TYPE_STR;
	j->buf[UDPC_SIG_OFFSET + 1] = hnlen;
	j->buf[UDPC_SIG_OFFSET + 2] = '\0';
	memcpy(j->buf + UDPC_SIG_OFFSET + 3, hn, hnlen);
	j->blen = UDPC_SIG_OFFSET + 3 + hnlen;
	send_cl(j);
	return;
}

static void
cli_hy_rpl(job_t j)
{
	UD_DEBUG("mod/cli: ->%s HY\n", hn);
	return;
}

/* the LS service */
static void
cli_ls(job_t j)
{
	UD_DEBUG("mod/cli: %s<- LS\n", hn);
	udpc_make_rpl_pkt(JOB_PACKET(j));
	j->buf[8] = UDPC_TYPE_STR;
	j->buf[9] = hnlen;
	memcpy(j->buf + 10, hn, hnlen);
	j->blen = 8 + 1 + 1 + hnlen;
	send_cl(j);
	return;
}

static void
cli_ls_rpl(job_t j)
{
	UD_DEBUG("mod/cli: ->%s HY\n", hn);
	return;
}


void
init(void *clo)
{
	UD_DEBUG("mod/cli: loading ...");

	/* obtain the hostname */
	(void)gethostname(hn, sizeof(hn));
	hnlen = strlen(hn);

	/* lodging our HY service */
	ud_set_service(0x1336, cli_hy, cli_hy_rpl);
	/* lodging the LS service */
	ud_set_service(0x1330, cli_ls, cli_ls_rpl);

	UD_DBGCONT("done\n");
	return;
}

void
reinit(void *clo)
{
	UD_DEBUG("mod/cli: reloading ...");
	UD_DBGCONT("done\n");
	return;
}

void
deinit(void *clo)
{
	UD_DEBUG("mod/cli: unloading ...");
	UD_DBGCONT("done\n");
	return;
}

/* dso-cli.c ends here */
