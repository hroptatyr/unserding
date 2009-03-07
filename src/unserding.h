/*** unserding.h -- unserding network service
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

#if !defined INCLUDED_unserding_h_
#define INCLUDED_unserding_h_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define UD_NETWORK_SERVICE	8653
#define UD_NETWORK_SERVSTR	"8653"
/* 239.0.0.0/8 are organisational solicited v4 mcast addrs */
#define UD_MCAST4_ADDR		"239.86.53.1"
#define UD_MCAST4S2S_ADDR	"239.86.53.3"
/* ff3x::8000:0-ff3x::ffff:ffff - dynamically allocated by hosts when needed */
#define UD_MCAST6_ADDR		"ff38:8653::1"
#define UD_MCAST6S2S_ADDR	"ff38:8653::3"

/* should be computed somehow using the mtu of the nic */
#define UDPC_SIMPLE_PKTLEN	4096

/**
 * Flags. */
typedef long unsigned int ud_flags_t;

/**
 * The catalogue data type. */
typedef struct ud_cat_s *ud_cat_t;

/** Flag to indicate this is merely a `branch holder'. */
#define UD_CF_JUSTCAT		0x01
/** Flag to indicate a spot value could be obtained (current quote). */
#define UD_CF_SPOTTABLE		0x02
/** Flag to indicate a bid and ask price could be obtained. */
#define UD_CF_TRADABLE		0x04
/** Flag to indicate a last trade can be obtained. */
#define UD_CF_LAST		0x08
/** Flag to indicate */


/**
 * Connexion handle to an unserding network.
 * Carries the current conversation id which is incremented upon each query
 * and hence one handle may not be used concurrently, or IOW it is not
 * thread-safe.
 * For multiple concurrent connexions obtain multiple handles. */
typedef struct ud_handle_s *ud_handle_t;
/**
 * Simple packet structure.
 * Attention, this is a structure passed by value. */
typedef struct {
	size_t plen;
	char *pbuf;
} ud_packet_t;

/**
 * Structure to chain several packets. */
typedef ud_packet_t *ud_pktchn_t;

/**
 * Conversation id. */
typedef uint8_t ud_convo_t;
/**
 * Packet command. */
typedef uint16_t ud_pkt_cmd_t;
/**
 * Packet number. */
typedef uint32_t ud_pkt_no_t;

struct ud_handle_s {
	/** Conversation number. */
	ud_convo_t convo;
	/** Socket. */
	int sock:24;
	int epfd;
	ud_pktchn_t pktchn;
};

/* connexion stuff */
/**
 * Initialise a handle into what's behind HDL. */
extern void init_unserding_handle(ud_handle_t hdl);
/**
 * Free a handle and all resources. */
extern void free_unserding_handle(ud_handle_t);
/**
 * Send packet PKT through the handle HDL.
 * Block until there is an answer. */
extern void ud_send_raw(ud_handle_t hdl, ud_packet_t pkt);
/**
 * Send a CMD-packet through the handle HDL. */
extern ud_convo_t ud_send_simple(ud_handle_t hdl, ud_pkt_cmd_t cmd);
/**
 * Wait (read block) until packets or TIMEOUT millisecs have passed. */
extern void
ud_recv_raw(ud_handle_t hdl, ud_packet_t pkt, int timeout);
/**
 * Like ud_recv_raw() but only receive packets that belong to convo CNO. */
extern void
ud_recv_convo(ud_handle_t hdl, ud_packet_t *pkt, int timeout, ud_convo_t cno);
/**
 * Return the current conversation id of HDL. */
extern inline ud_convo_t __attribute__((always_inline, gnu_inline))
ud_handle_convo(ud_handle_t hdl);
/**
 * Return the connexion socket stored inside HDL. */
extern inline int __attribute__((always_inline, gnu_inline))
ud_handle_sock(ud_handle_t hdl);

/* inlines */
extern inline ud_convo_t __attribute__((always_inline, gnu_inline))
ud_handle_convo(ud_handle_t hdl)
{
	return hdl->convo;
}	

extern inline int __attribute__((always_inline, gnu_inline))
ud_handle_sock(ud_handle_t hdl)
{
	return hdl->sock;
}	


/* catalogue codswallop */
typedef uint8_t ud_tag_t;
typedef struct ud_catobj_s *ud_catobj_t;

extern void *instruments;

/* catalogue entries look like this
 * basically a proper tlv cell except sometimes the length is implicit */
typedef struct ud_tlv_s *ud_tlv_t;
typedef struct ud_tlvcons_s *ud_tlvcons_t;


extern void ud_cat_add_obj(ud_catobj_t co);

/* instruments */
extern void init_instr(void);

#endif	/* INCLUDED_unserding_h_ */
