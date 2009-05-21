/*** protocore.c -- unserding protocol guts
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
#endif
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
#include "protocore-private.h"

#if defined UNSERSRV
/**
 * Big array with worker functions, aka services.
 * For now we support 65536 services, of which only the even ones may be
 * used to raise questions to the service and the next odd one will be
 * the reply. */
static ud_pktwrk_f ud_services[65536];


void
ud_proto_parse(job_t j)
{
	ud_pkt_cmd_t cmd = udpc_pkt_cmd((ud_packet_t){0, j->buf});
	ud_pktwrk_f wf = ud_services[cmd];

	if (UNLIKELY(wf == NULL)) {
		UD_LOG("found 0x%04x but cannot cope\n", cmd);
		return;
	}
	/* otherwise, just do what's in there */
	wf(j);
	return;
}

/* In real life we probably want a list of workers and the module does not
 * need to know about the previous worker function for CMD.  Ideally we
 * do not expose the function ptr at all, instead return a pointer into
 * the services array and provide a fun like next() to allow for defadvice'd
 * functions. */
extern void
ud_set_service(ud_pkt_cmd_t cmd, ud_pktwrk_f fun, ud_pktwrk_f rpl);
void
ud_set_service(ud_pkt_cmd_t cmd, ud_pktwrk_f fun, ud_pktwrk_f rpl)
{
	cmd &= ~1;
	ud_services[cmd | 0] = fun;
	ud_services[cmd | 1] = rpl;
	return;
}


void
init_proto(void)
{
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
	uint16_t i = 0;
	for (; i < (pkt.plen & ~0xf); i += 16) {
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
	if (i == pkt.plen) {
		return;
	}

	fprintf(fp, "%04x  ", i);
	if ((pkt.plen & 0xf) >= 8) {
		putb(pkt.pbuf[i+0], fp);
		putb(pkt.pbuf[i+1], fp);
		putb(pkt.pbuf[i+2], fp);
		putb(pkt.pbuf[i+3], fp);
		putb(pkt.pbuf[i+4], fp);
		putb(pkt.pbuf[i+5], fp);
		putb(pkt.pbuf[i+6], fp);
		putb(pkt.pbuf[i+7], fp);
		putc(' ', fp);
		i += 8;
	}
	/* and the rest, duff's */
	switch (pkt.plen & 0x7) {
	case 7:
		putb(pkt.pbuf[i++], fp);
	case 6:
		putb(pkt.pbuf[i++], fp);
	case 5:
		putb(pkt.pbuf[i++], fp);
	case 4:
		putb(pkt.pbuf[i++], fp);

	case 3:
		putb(pkt.pbuf[i++], fp);
	case 2:
		putb(pkt.pbuf[i++], fp);
	case 1:
		putb(pkt.pbuf[i++], fp);
	case 0:
	default:
		break;
	}
	putc('\n', fp);
	return;
}

static uint16_t /* better not inline */
__fprint_one(const char *buf, FILE *fp)
{
	uint16_t len = 0;

	switch ((udpc_type_t)buf[0]) {
	case UDPC_TYPE_STR:
		fputs("(string)", fp);
		ud_fputs(buf[1], &buf[2], fp);
		len = buf[1] + 2;
		break;

	case UDPC_TYPE_VAR:
		fputs("(data)", fp);
		for (uint8_t i = 2; i < buf[1] + 2; i++) {
			fprintf(fp, "(%02x)", buf[i]);
		}
		len = buf[1] + 2;
		break;

	case UDPC_TYPE_SEQ:
		fprintf(fp, "(seqof(#%d))", buf[1]);
		len = 2;

	case UDPC_TYPE_SI32:
	case UDPC_TYPE_UI32: {
		unsigned int dw = *(const unsigned int*const)&buf[1];
		fprintf(fp, "(dword)%08x", dw);
		len = 1 + sizeof(dw);
		break;
	}

	case UDPC_TYPE_SI16:
	case UDPC_TYPE_UI16: {
		short unsigned int dw = *(const unsigned int*const)&buf[1];
		fprintf(fp, "(word)%04x", dw);
		len = 1 + sizeof(dw);
		break;
	}
	case UDPC_TYPE_UNK:
	default:
		fprintf(fp, "(%02x)", buf[0]);
		len = 1;
		break;
	}
	return len;
}

void
ud_fprint_pkt_pretty(ud_packet_t pkt, FILE *fp)
{
	for (uint16_t i = 8; i < pkt.plen; i += __fprint_one(&pkt.pbuf[i], fp));
	putc('\n', fp);
	return;
}

/* protocore.c ends here */
