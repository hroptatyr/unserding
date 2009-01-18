/*** protocore.c -- unserding protocol guts
 *
 * Copyright (C) 2008 Sebastian Freundt
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* posix? */
#include <limits.h>
#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"

/**
 * Big array with worker functions.
 * 256 families should suffice methinks. */
extern ud_pktfam_t ud_pktfam[];
/**
 * Family 00, general adminitrative procedures. */
extern ud_pktwrk_f ud_fam00[];
/**
 * Family 01, catalogue procs. */
extern ud_pktwrk_f ud_fam01[];
/**
 * Family 7e, test stuff. */
extern ud_pktwrk_f ud_fam7e[];


ud_pktfam_t ud_pktfam[128] = {
	/* family 0 */
	ud_fam00,
	ud_fam01,
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	/* fam 16 */
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	/* fam 32 */
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	/* fam 48 */
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	/* fam 64 */
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	/* fam 80 */
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	/* fam 96 */
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	/* fam 112 */
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, ud_fam7e, NULL,
};

/* forwarders */
static void f00_hy(job_t j);
static void f00_hy_rpl(job_t j);

ud_pktwrk_f ud_fam00[256] = {
	f00_hy, f00_hy_rpl,
};

static void f01_ls(job_t j);
static void f01_ls_rpl(job_t j);

ud_pktwrk_f ud_fam01[256] = {
	f01_ls, f01_ls_rpl,
};

static void f7e_54(job_t j);

ud_pktwrk_f ud_fam7e[256] = {
	/* 0 */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/* 16 */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/* 32 */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/* 48 */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/* 64 */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/* 80 */
	NULL, NULL, NULL, NULL, f7e_54, NULL /* no rpl */, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};


/* family 00 */

/* HY packet */
static size_t neighbours = 0;

#define MAXHOSTNAMELEN		64
/* text section */
unsigned char hnlen;
static char hn[MAXHOSTNAMELEN] = "";

static void
f00_hy(job_t j)
{
	UD_DEBUG_PROTO("found HY\n");
	/* generate the answer packet */
	udpc_make_rpl_pkt(JOB_PACKET(j));
	UD_DEBUG_PROTO("sending HY RPL\n");
	/* just say that there's more in this packet which is a string */
	j->buf[8] = UDPC_TYPE_STRING;
	/* attach the hostname now */
	j->buf[9] = hnlen;
	memcpy(j->buf + 10, hn, hnlen);
	j->blen = 8 + 1 + 1 + hnlen;
	/* initialise the neighbours counter */
	neighbours = 0;
	/* and send him back */
	send_cl(j);
	return;
}

/* handle the HY RPL packet */
static void
f00_hy_rpl(job_t j)
{
	UD_DEBUG_PROTO("found HY RPL\n");
	neighbours++;
	return;
}

static void
f7e_54(job_t j)
{
	/* just a 2.5 seconds delayed HY */
	usleep(2500000);
	/* generate the answer packet */
	udpc_make_rpl_pkt(JOB_PACKET(j));
	UD_DEBUG_PROTO("sending 54 RPL\n");
	/* and send him back */
	send_cl(j);
	return;
}

static void
f01_ls(job_t j)
{
}

static void
f01_ls_rpl(job_t j)
{
}


/* family 01 */


void
ud_proto_parse(job_t j)
{
	ud_pkt_cmd_t cmd = udpc_pkt_cmd((ud_packet_t){0, j->buf});
	uint8_t fam = udpc_cmd_fam(cmd);
	uint8_t wrk = udpc_cmd_wrk(cmd);
	ud_pktfam_t pf = ud_pktfam[fam];
	ud_pktwrk_f wf = pf ? pf[wrk] : NULL;

	if (UNLIKELY(wf == NULL)) {
		UD_LOG("found 0x%04x but cannot cope\n", cmd);
		return;
	}
	/* otherwise, just do what's in there */
	wf(j);
	return;
}

void
init_proto(void)
{
	/* obtain the hostname */
	(void)gethostname(hn, countof(hn));
	hnlen = strlen(hn);
	return;
}

/* protocore.c ends here */
