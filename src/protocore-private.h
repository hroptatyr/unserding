/*** protocore.h -- unserding protocol guts
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

#if !defined INCLUDED_protocore_private_h_
#define INCLUDED_protocore_private_h_

#include <stdio.h>
#include "protocore.h"

#if defined __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif /* __cplusplus */

static inline void __attribute__((always_inline))
ud_fputs(uint8_t len, const char *s, FILE *f)
{
	for (uint8_t i = 0; i < len; i++) {
		putc_unlocked(s[i], f);
	}
	return;
}

extern size_t ud_sprint_pkthdr(char *restrict buf, ud_packet_t pkt);
extern size_t ud_sprint_pkt_raw(char *restrict buf, ud_packet_t pkt);
extern size_t ud_sprint_pkt_pretty(char *restrict buf, ud_packet_t pkt);


/* inlines */
#if defined __INTEL_COMPILER
/* args are eval'd in unspecified order */
#pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */
/**
 * Print the packet header. temporary. */
static inline void __attribute__((always_inline))
udpc_print_pkt(const ud_packet_t pkt)
{
	printf(":len %04x :cno %02x :pno %06x :cmd %04x :mag %04x\n",
	       (unsigned int)pkt.plen,
	       udpc_pkt_cno(pkt), udpc_pkt_pno(pkt), udpc_pkt_cmd(pkt),
	       ntohs(((const uint16_t*)pkt.pbuf)[3]));
	return;
}

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_protocore_private_h_ */
