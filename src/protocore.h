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

#if !defined INCLUDED_protocore_h_
#define INCLUDED_protocore_h_

#include <stdbool.h>
#include <stdint.h>
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif	/* HAVE_NETINET_IN_H */

/***
 * The unserding protocol in detail:
 *
 * - A simple packet is always 4096 bytes long
 * - Simple packets always read:
 *   offs 0x000 uint8_t  CONVO	conversation id, can be set by the client
 *   offs 0x001 uint24_t PKTNO	packet number wrt to the conversation
 *   offs 0x004 uint16_t CMD	command to issue, see below
 *   offs 0x006 uint16_t PAD	just padding, must be magic number 0xbeef
 *
 * Commands (or packet types as we call them) have the property that a
 * requesting command (c2s) is even, the corresponding reply (if existent)
 * is then odd (LOR'd with 0x0001).
 *
 ***/

#define UDPC_MAGIC_NUMBER	(uint16_t)(htons(0xbeef))

/**
 * Return true if PKT is a valid unserding packet. */
extern inline bool __attribute__((always_inline, gnu_inline))
udpc_pkt_valid_p(const ud_packet_t pkt);
/**
 * Return true if the packet PKT is meant for us,
 * that is the conversation ids coincide. */
extern inline bool __attribute__((always_inline, gnu_inline))
udpc_pkt_for_us_p(const ud_packet_t pkt, ud_convo_t cno);
/**
 * Generic packet generator.
 * Create a packet into P with conversation number CID and packet
 * number PNO of type T. */
extern inline void __attribute__((always_inline, gnu_inline))
udpc_make_pkt(ud_packet_t p, ud_convo_t cno, ud_pkt_no_t pno, ud_pkt_cmd_t t);
/**
 * Generic packet generator.
 * Create a packet from the packet PKT into PKT, the packet command is XOR'd
 * with 0x1 and the packet number is increased by 1. */
extern inline void __attribute__((always_inline, gnu_inline))
udpc_make_rpl_pkt(ud_packet_t pkt);
/**
 * Extract the conversation number from PKT. */
extern inline ud_convo_t __attribute__((always_inline, gnu_inline))
udpc_pkt_cno(const ud_packet_t pkt);
/**
 * Extract the packet number from PKT. */
extern inline ud_pkt_no_t __attribute__((always_inline, gnu_inline))
udpc_pkt_pno(const ud_packet_t pkt);
/**
 * Extract the command portion of PKT. */
extern inline ud_pkt_cmd_t __attribute__((always_inline, gnu_inline))
udpc_pkt_cmd(const ud_packet_t pkt);
/**
 * Print the packet header. temporary. */
extern inline void __attribute__((always_inline, gnu_inline))
udpc_print_pkt(const ud_packet_t pkt);
/**
 * Return true if CMD is a reply command. */
extern inline bool __attribute__((always_inline, gnu_inline))
udpc_reply_p(ud_pkt_cmd_t cmd);
/**
 * Given a command CMD, return the corresponding reply command. */
extern inline ud_pkt_cmd_t __attribute__((always_inline, gnu_inline))
udpc_reply_cmd(ud_pkt_cmd_t cmd);
/**
 * Given a command CMD in network byte order return its reply command. */
extern inline ud_pkt_cmd_t __attribute__((always_inline, gnu_inline))
udpc_reply_cmd_ns(ud_pkt_cmd_t cmd);


/* inlines */
extern inline bool __attribute__((always_inline, gnu_inline))
udpc_pkt_valid_p(const ud_packet_t pkt)
{
	const uint16_t *tmp = (const void*)pkt.pbuf;
	/* check magic number */
	if (tmp[3] == UDPC_MAGIC_NUMBER) {
		return true;
	}
	return false;
}

extern inline bool __attribute__((always_inline, gnu_inline))
udpc_pkt_for_us_p(const ud_packet_t pkt, ud_convo_t cno)
{
	return udpc_pkt_valid_p(pkt) && udpc_pkt_cno(pkt) == cno;
}

extern inline void __attribute__((always_inline, gnu_inline))
udpc_print_pkt(const ud_packet_t pkt)
{
	printf(":len %04x :cno %02x :pno %06x :cmd %04x :mag %04x\n",
	       (unsigned int)pkt.plen,
	       udpc_pkt_cno(pkt), udpc_pkt_pno(pkt), udpc_pkt_cmd(pkt),
	       ntohs(((const uint16_t*)pkt.pbuf)[3]));
	return;
}

extern inline uint32_t __attribute__((always_inline, gnu_inline))
__interleave_cno_pno(ud_convo_t cno, ud_pkt_no_t pno);
extern inline uint32_t __attribute__((always_inline, gnu_inline))
__interleave_cno_pno(ud_convo_t cno, ud_pkt_no_t pno)
{
	return ((uint32_t)cno << 24) | (pno && 0xffffff);
}

extern inline void __attribute__((always_inline, gnu_inline))
udpc_make_pkt(ud_packet_t p, ud_convo_t cno, ud_pkt_no_t pno, ud_pkt_cmd_t cmd)
{
	uint16_t *restrict tmp = (void*)p.pbuf;
	uint32_t *restrict tm2 = (void*)p.pbuf;
	tm2[0] = htonl(__interleave_cno_pno(cno, pno));
	tmp[2] = htons(cmd);
	tmp[3] = UDPC_MAGIC_NUMBER;
	return;
}

extern inline bool __attribute__((always_inline, gnu_inline))
udpc_reply_p(ud_pkt_cmd_t cmd)
{
	return cmd & 0x1;
}

extern inline ud_pkt_cmd_t __attribute__((always_inline, gnu_inline))
udpc_reply_cmd(ud_pkt_cmd_t cmd)
{
	return cmd | 0x1;
}

extern inline ud_pkt_cmd_t __attribute__((always_inline, gnu_inline))
udpc_reply_cmd_ns(ud_pkt_cmd_t cmd)
{
	ud_pkt_cmd_t res = udpc_reply_cmd(ntohs(cmd));
	return htons(res);
}

extern inline void __attribute__((always_inline, gnu_inline))
udpc_make_rpl_pkt(ud_packet_t p)
{
	uint16_t *restrict tmp = (void*)p.pbuf;
	uint32_t *restrict tm2 = (void*)p.pbuf;
	uint32_t all = ntohl(tm2[0]);

	/* increment the pkt number */
	tm2[0] = htonl(all+1);
	/* construct the reply packet type */
	tmp[2] = udpc_reply_cmd_ns(tmp[2]);
	/* voodoo */
	tmp[3] = UDPC_MAGIC_NUMBER;
	return;
}

extern inline ud_convo_t __attribute__((always_inline, gnu_inline))
udpc_pkt_cno(const ud_packet_t pkt)
{
	const uint32_t *tmp = (void*)pkt.pbuf;
	uint32_t all = ntohl(tmp[0]);
	/* yes ntoh conversion!!! */
	return (ud_convo_t)(all >> 24);
}

extern inline ud_pkt_no_t __attribute__((always_inline, gnu_inline))
udpc_pkt_pno(const ud_packet_t pkt)
{
	const uint32_t *tmp = (void*)pkt.pbuf;
	uint32_t all = ntohl(tmp[0]);
	/* yes ntoh conversion!!! */
	return (ud_pkt_no_t)(all & (0xffffff));
}

extern inline ud_pkt_cmd_t __attribute__((always_inline, gnu_inline))
udpc_pkt_cmd(const ud_packet_t pkt)
{
	const uint16_t *tmp = (void*)pkt.pbuf;
	/* yes ntoh conversion!!! */
	return ntohs(tmp[2]);
}

#endif	/* INCLUDED_protocore_h_ */
