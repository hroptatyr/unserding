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

#if defined UNSERSRV
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
	j->buf[8] = UDPC_TYPE_UNK;
	j->blen = 9;
	UD_DEBUG_PROTO("sending 54 RPL\n");
	/* and send him back */
	send_cl(j);
	return;
}

extern bool ud_cat_ls_job(job_t j);

static void
f01_ls(job_t j)
{
#if 0
	while (ud_cat_ls_job(j)) {
		/* and send him back */
		send_cl(j);
	}
#else
	ud_cat_ls_job(j);
	/* and send him back */
	send_cl(j);
#endif
	return;
}

static void __attribute__((unused))
f01_ls_rpl(job_t j)
{
	/* just ignore hime */
	return;
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
#endif	/* UNSERSRV */

void
ud_fprint_pkthdr(ud_packet_t pkt, FILE *fp)
{
	fprintf(fp, ":len %04x :cno %02x :pno %06x :cmd %04x :mag %04x\n",
		(unsigned int)pkt.plen,
		udpc_pkt_cno(pkt),
		udpc_pkt_pno(pkt),
		udpc_pkt_cmd(pkt),
		ntohs(((const uint16_t*)pkt.pbuf)[3]));
	return;
}

static inline void __attribute__((always_inline, gnu_inline))
putb(char a, FILE *fp)
{
	char b = a & 0xf, c = (a >> 4) & 0xf;
	switch (c) {
	case 0 ... 9:
		putc(c + 0x30, fp);
		break;
	case 10 ... 15:
		putc(c + 0x57, fp);
		break;
	default:
		break;
	}
	switch (b) {
	case 0 ... 9:
		putc(b + 0x30, fp);
		break;
	case 10 ... 15:
		putc(b + 0x57, fp);
		break;
	default:
		break;
	}
	putc(' ', fp);
	return;
}

void
ud_fprint_pkt_raw(ud_packet_t pkt, FILE *fp)
{
	for (uint16_t i = 0; i < pkt.plen; i += 16) {
		fprintf(fp, "%04x  ", i);
		putb(pkt.pbuf[i+0], fp);
		putb(pkt.pbuf[i+1], fp);
		putb(pkt.pbuf[i+2], fp);
		putb(pkt.pbuf[i+3], fp);
		putb(pkt.pbuf[i+4], fp);
		putb(pkt.pbuf[i+5], fp);
		putb(pkt.pbuf[i+6], fp);
		putb(pkt.pbuf[i+7], fp);
		putc(' ', fp);
		putb(pkt.pbuf[i+8], fp);
		putb(pkt.pbuf[i+9], fp);
		putb(pkt.pbuf[i+10], fp);
		putb(pkt.pbuf[i+11], fp);
		putb(pkt.pbuf[i+12], fp);
		putb(pkt.pbuf[i+13], fp);
		putb(pkt.pbuf[i+14], fp);
		putb(pkt.pbuf[i+15], fp);
		putc('\n', fp);
	}
	if ((pkt.plen & 0xf) > 0) {
		uint16_t i = (pkt.plen & ~0xf) + 16;
		
		fprintf(fp, "%04x  ", i);
		switch (pkt.plen & 0xf) {
		case 15:
			putb(pkt.pbuf[i++], fp);
		case 14:
			putb(pkt.pbuf[i++], fp);
		case 13:
			putb(pkt.pbuf[i++], fp);
		case 12:
			putb(pkt.pbuf[i++], fp);

		case 11:
			putb(pkt.pbuf[i++], fp);
		case 10:
			putb(pkt.pbuf[i++], fp);
		case 9:
			putb(pkt.pbuf[i++], fp);
		case 8:
			putb(pkt.pbuf[i++], fp);

		case 7:
			!(i & 7) ? putc(' ', stdout) : 0;
			putb(pkt.pbuf[i++], fp);
		case 6:
			!(i & 7) ? putc(' ', stdout) : 0;
			putb(pkt.pbuf[i++], fp);
		case 5:
			!(i & 7) ? putc(' ', stdout) : 0;
			putb(pkt.pbuf[i++], fp);
		case 4:
			!(i & 7) ? putc(' ', stdout) : 0;
			putb(pkt.pbuf[i++], fp);

		case 3:
			!(i & 7) ? putc(' ', stdout) : 0;
			putb(pkt.pbuf[i++], fp);
		case 2:
			!(i & 7) ? putc(' ', stdout) : 0;
			putb(pkt.pbuf[i++], fp);
		case 1:
			!(i & 7) ? putc(' ', stdout) : 0;
			putb(pkt.pbuf[i++], fp);
		case 0:
		default:
			break;
		}
		putc('\n', fp);
	}
#if 0
	/* generic packet printer, doesnt belong here */
	for (uint16_t i = 8, len = 0; len < pkt.plen; i += len) {
		/* two spaces upfront */
		putc_unlocked(' ', stdout);
		putc_unlocked(' ', stdout);
		/* deseriealise him */
		if (UNLIKELY((len = deserialise(&pkt.pbuf[i], stdout)) == 0)) {
			break;
		}
	}
#endif
	return;
}

/* protocore.c ends here */
