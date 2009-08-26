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
/* posix? */
#include <inttypes.h>

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
void
ud_set_service(ud_pkt_cmd_t cmd, ud_pktwrk_f fun, ud_pktwrk_f rpl)
{
	cmd &= ~1;
	ud_services[cmd | 0] = fun;
	ud_services[cmd | 1] = rpl;
	return;
}

ud_pktwrk_f
ud_get_service(ud_pkt_cmd_t cmd)
{
	return ud_services[cmd];
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

size_t
ud_sprint_pkthdr(char *restrict buf, ud_packet_t pkt)
{
	return sprintf(buf,
		       ":len %04x :cno %02x :pno %06x :cmd %04x :mag %04x\n",
		       (unsigned int)pkt.plen,
		       udpc_pkt_cno(pkt),
		       udpc_pkt_pno(pkt),
		       udpc_pkt_cmd(pkt),
		       ntohs(((const uint16_t*)pkt.pbuf)[3]));
}

static inline void __attribute__((always_inline, gnu_inline))
b2a(char *restrict outbuf, char a)
{
	char b = a & 0xf, c = (a >> 4) & 0xf;
	switch (c) {
	case 0 ... 9:
		outbuf[0] = c + '0';
		break;
	case 10 ... 15:
		outbuf[0] = c + 0x57;
		break;
	default:
		break;
	}
	switch (b) {
	case 0 ... 9:
		outbuf[1] = b + '0';
		break;
	case 10 ... 15:
		outbuf[1] = b + 0x57;
		break;
	default:
		break;
	}
	outbuf[2] = ' ';
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

size_t
ud_sprint_pkt_raw(char *restrict buf, ud_packet_t pkt)
{
	size_t res = 0;
	uint16_t i = 0;

	for (; i < (pkt.plen & ~0xf); i += 16) {
		res += sprintf(&buf[res], "%04x  ", i);
		b2a(&buf[res + 0], pkt.pbuf[i+0]);
		b2a(&buf[res + 3], pkt.pbuf[i+1]);
		b2a(&buf[res + 6], pkt.pbuf[i+2]);
		b2a(&buf[res + 9], pkt.pbuf[i+3]);
		b2a(&buf[res + 12], pkt.pbuf[i+4]);
		b2a(&buf[res + 15], pkt.pbuf[i+5]);
		b2a(&buf[res + 18], pkt.pbuf[i+6]);
		b2a(&buf[res + 21], pkt.pbuf[i+7]);
		buf[res + 24] = ' ';
		b2a(&buf[res + 25], pkt.pbuf[i+8]);
		b2a(&buf[res + 28], pkt.pbuf[i+9]);
		b2a(&buf[res + 31], pkt.pbuf[i+10]);
		b2a(&buf[res + 34], pkt.pbuf[i+11]);
		b2a(&buf[res + 37], pkt.pbuf[i+12]);
		b2a(&buf[res + 40], pkt.pbuf[i+13]);
		b2a(&buf[res + 43], pkt.pbuf[i+14]);
		b2a(&buf[res + 46], pkt.pbuf[i+15]);
		buf[res + 49] = '\n';
		res += 50;
	}
	if (i == pkt.plen) {
		return res;
	}

	res += sprintf(&buf[res], "%04x  ", i);
	if ((pkt.plen & 0xf) >= 8) {
		b2a(&buf[res + 0], pkt.pbuf[i+0]);
		b2a(&buf[res + 3], pkt.pbuf[i+1]);
		b2a(&buf[res + 6], pkt.pbuf[i+2]);
		b2a(&buf[res + 9], pkt.pbuf[i+3]);
		b2a(&buf[res + 12], pkt.pbuf[i+4]);
		b2a(&buf[res + 15], pkt.pbuf[i+5]);
		b2a(&buf[res + 18], pkt.pbuf[i+6]);
		b2a(&buf[res + 21], pkt.pbuf[i+7]);
		buf[res + 24] = ' ';
		i += 8;
		res += 25;
	}
	/* and the rest, duff's */
	switch (pkt.plen & 0x7) {
	case 7:
		b2a(&buf[res], pkt.pbuf[i++]);
		res += 3;
	case 6:
		b2a(&buf[res], pkt.pbuf[i++]);
		res += 3;
	case 5:
		b2a(&buf[res], pkt.pbuf[i++]);
		res += 3;
	case 4:
		b2a(&buf[res], pkt.pbuf[i++]);
		res += 3;

	case 3:
		b2a(&buf[res], pkt.pbuf[i++]);
		res += 3;
	case 2:
		b2a(&buf[res], pkt.pbuf[i++]);
		res += 3;
	case 1:
		b2a(&buf[res], pkt.pbuf[i++]);
		res += 3;
	case 0:
	default:
		break;
	}
	buf[res++] = '\n';
	return res;
}

static size_t
__pretty_oneseq(char *restrict buf, udpc_seria_t sctx, uint8_t tag)
{
	size_t res = 0;

	switch (tag) {
	case UDPC_TYPE_UI16:
	case UDPC_TYPE_SI16: {
		const uint16_t *v;
		size_t len = udpc_seria_des_sequi16(sctx, &v);
		res = sprintf(buf, "seqof(xi16) * %d:\n", (int)len);
		for (index_t i = 0; i < len; i++) {
			res += sprintf(&buf[res], "  %04x (%d)\n", v[i], v[i]);
		}
		break;
	}

	case UDPC_TYPE_UI32:
	case UDPC_TYPE_SI32: {
		const uint32_t *v;
		size_t len = udpc_seria_des_sequi32(sctx, &v);
		res = sprintf(buf, "seqof(xi32) * %d:\n", (int)len);
		for (index_t i = 0; i < len; i++) {
			res += sprintf(&buf[res], "  %08x (%d)\n", v[i], v[i]);
		}
		break;
	}

	case UDPC_TYPE_UI64:
	case UDPC_TYPE_SI64: {
		const uint64_t *v;
		size_t len = udpc_seria_des_sequi64(sctx, &v);
		res = sprintf(buf, "seqof(xi64) * %d:\n", (int)len);
		for (index_t i = 0; i < len; i++) {
			res += sprintf(&buf[res],
				       "  %016" PRIx64 " (%" PRId64 ")\n",
				       v[i], v[i]);
		}
		break;
	}

	case UDPC_TYPE_FLTS: {
		const float *v;
		size_t len = udpc_seria_des_seqflts(sctx, &v);
		res = sprintf(buf, "seqof(flts) * %d:\n", (int)len);
		for (index_t i = 0; i < len; i++) {
			res += sprintf(&buf[res], "  %f\n", v[i]);
		}
		break;
	}

	case UDPC_TYPE_FLTD: {
		const double *v;
		size_t len = udpc_seria_des_seqfltd(sctx, &v);
		res = sprintf(buf, "seqof(fltd) * %d:\n", (int)len);
		for (index_t i = 0; i < len; i++) {
			res += sprintf(&buf[res], "  %f\n", v[i]);
		}
		break;
	}

	case UDPC_TYPE_FLTH:
	default:
		memcpy(buf, "(unknown)\n", 10);
		return 10;
	}
	return res;
}

static size_t
__pretty_one(char *restrict buf, udpc_seria_t sctx, uint8_t tag)
{
	if ((tag & UDPC_SEQ_MASK) && tag != UDPC_TYPE_STR) {
		return __pretty_oneseq(buf, sctx, tag & ~UDPC_SEQ_MASK);
	}

	switch (tag) {
	case UDPC_TYPE_BYTE:
	case (UDPC_TYPE_BYTE | UDPC_SGN_MASK):
		memcpy(buf, "(byte)", 6);
		b2a(buf + 6, udpc_seria_des_byte(sctx));
		buf[8] = '\n';
		return 9;

	case UDPC_TYPE_STR: {
		const char *s;
		size_t len;

		memcpy(buf, "(string)", 8);
		buf[8] = '"';
		len = udpc_seria_des_str(sctx, &s);
		memcpy(buf + 9, s, len);
		buf[len + 9] = '"';
		buf[len + 10] = '\n';
		return len + 11;
	}

	case UDPC_TYPE_DATA: {
		const char *s;
		uint8_t len;

		memcpy(buf, "(DATA)", 6);
		len = udpc_seria_des_data(sctx, (const void**)&s);
		len = sprintf(buf + 6, "%u\n", len);
		return len + 6;
	}

	case UDPC_TYPE_XDR: {
		const char *s;
		size_t len;

		memcpy(buf, "(XDR)", 5);
		len = udpc_seria_des_xdr(sctx, (const void**)&s);
		len = sprintf(buf + 5, "%lu\n", (long unsigned int)len);
		return len + 5;
	}

	case UDPC_TYPE_ASN1: {
		const char *s;
		size_t len;

		memcpy(buf, "(ASN.1)", 7);
		len = udpc_seria_des_asn1(sctx, (const void**)&s);
		len = sprintf(buf + 7, "%lu\n", (long unsigned int)len);
		return len + 7;
	}

	case UDPC_TYPE_UI16: {
		uint16_t v = udpc_seria_des_ui16(sctx);
		return sprintf(buf, "(ui16)0x%04x (%u)\n", v, v);
	}
	case UDPC_TYPE_SI16: {
		int16_t v = udpc_seria_des_si16(sctx);
		return sprintf(buf, "(si16)0x%04x (%d)\n", v, v);
	}

	case UDPC_TYPE_UI32: {
		uint32_t v = udpc_seria_des_ui32(sctx);
		return sprintf(buf, "(ui32)0x%08x (%u)\n", v, v);
	}
	case UDPC_TYPE_SI32: {
		int32_t v = udpc_seria_des_si32(sctx);
		return sprintf(buf, "(si32)0x%08x (%u)\n", v, v);
	}


	case UDPC_TYPE_UI64: {
		uint64_t v = udpc_seria_des_ui64(sctx);
		return sprintf(buf, "(ui64)0x%16llx (%llu)\n",
			       (long long unsigned int)v,
			       (long long unsigned int)v);
	}
	case UDPC_TYPE_SI64: {
		uint64_t v = udpc_seria_des_si64(sctx);
		return sprintf(buf, "(si64)0x%16llx (%lld)\n",
			       (long long int)v,
			       (long long int)v);
	}

		
	case UDPC_TYPE_FLTS: {
		float v = udpc_seria_des_flts(sctx);
		return sprintf(buf, "(flts)%f\n", v);
	}

	case UDPC_TYPE_FLTD: {
		double v = udpc_seria_des_fltd(sctx);
		return sprintf(buf, "(fltd)%f\n", v);
	}

	case UDPC_TYPE_FLTH:
	default:
		fprintf(stderr, "type %d\n", tag);
		memcpy(buf, "(unknown)\n", 10);
		return 10;
	}
}

size_t
ud_sprint_pkt_pretty(char *restrict buf, ud_packet_t pkt)
{
	size_t res = 0;
	struct udpc_seria_s __sctx;
	udpc_seria_t sctx = &__sctx;
	uint8_t tag;

	udpc_seria_init(sctx, UDPC_PAYLOAD(pkt.pbuf), pkt.plen - UDPC_HDRLEN);

	while ((tag = udpc_seria_tag(sctx))) {
		res += __pretty_one(&buf[res], sctx, tag);
	}
	return res;
}

/* protocore.c ends here */
