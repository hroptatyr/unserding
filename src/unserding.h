/*** unserding.h -- unserding network service
 *
 * Copyright (C) 2008-2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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
#include <sys/types.h>
#if !defined UD_NEW_API || defined UD_COMPAT
# include "ud-sockaddr.h"
# include "mcast.h"
#endif	/* !UD_NEW_API || UD_COMPAT */

#if defined __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif /* __cplusplus */

#define UD_NETWORK_SERVICE	8364/*UDNG on the phone*/

/* http://www.iana.org/assignments/ipv6-multicast-addresses/ lists us 
 * as ff0x:0:0:0:0:0:0:134 */
/* node-local */
#define UD_MCAST6_NODE_LOCAL	"ff01::134"
/* link-local */
#define UD_MCAST6_LINK_LOCAL	"ff02::134"
/* site-local */
#define UD_MCAST6_SITE_LOCAL	"ff05::134"

/* just offer a default address for some tools */
#define UD_MCAST6_ADDR		UD_MCAST6_SITE_LOCAL

#if defined UD_NEW_API
typedef struct ud_sock_s *ud_sock_t;
typedef const struct ud_sock_s *ud_const_sock_t;

typedef uint16_t ud_svc_t;

/**
 * Public type for unserding sockets. */
struct ud_sock_s {
	/** socket for I/O, can be used as if acquired by `socket()' */
	const int fd;
	/** generic socket indicator */
	const uint32_t fl;
	/** data ptr the library won't touch, for external use by user */
	void *data;
	/** beginning of private section */
	char priv[0];
};

/**
 * Message for packing and unpacking unserding packets. */
struct ud_msg_s {
	/** service channel, user negotiated, 0xff00 - 0xffff are reserved */
	ud_svc_t svc;
	/** pointer to data to serialise */
	const void *data;
	/** length of data block to serialise */
	size_t dlen;
};

/**
 * Options to be handed to `ud_socket()'. */
struct ud_sockopt_s {
	enum {
		UD_NONE = 0U,
		UD_PUB = 1U,
		UD_SUB = 2U,
		UD_PUBSUB = 3U,
	} mode;
	/** mode options, can be |'d as needed */
	enum {
		UD_MOPT_NONE,
		UD_MOPT_BIND_LOCALLY,
	} mode_opt;
	/** address to send/subscribe to, UD_MCAST6_SITE_LOCAL if NULL */
	const char *addr;
	/** service to send/subscribe to, UD_NETWORK_SERVICE if 0 */
	short unsigned int port;
};


/* first up socket ctoring and dtoring */
/**
 * Return a socket set up for PUB'ing or SUB'ing, according to OPT.
 * The socket returned in the FD slot of the result object can be used
 * as if acquired by `socket()' so in particular (e)polling is allowed.
 * The socket object must be closed by `ud_close()'.
 *
 * If ADDRESS and/or SERVICE in OPT is omitted, the site-local address
 * UD_MCAST6_SITE_LOCAL and/or UD_NETWORK_SERVICE is used.
 *
 * MODE must be one of the socket mode specifiers as defined above. */
extern ud_sock_t ud_socket(struct ud_sockopt_s opt);

/**
 * Close a socket and free associated resources. */
extern int ud_close(ud_sock_t sock);

/* next up packing/unpacking messages */
/**
 * Produce wire-representation of P (of size Z) in SOCK. */
extern int ud_pack(ud_sock_t sock, ud_svc_t svc, const void *p, size_t z);

/**
 * Produce wire-representation of MSG in SOCK. */
extern int ud_pack_msg(ud_sock_t sock, struct ud_msg_s msg);

/**
 * Flush buffered packs immediately. */
extern int ud_flush(ud_sock_t sock);

/**
 * Read messages from SOCK and return a deserialised version in TGT. */
extern ssize_t
ud_chck(ud_svc_t *restrict svc, void *restrict tgt, size_t tsz, ud_sock_t sock);

/**
 * Read messages from SOCK and return a deserialised version in TGT. */
extern int ud_chck_msg(struct ud_msg_s *restrict tgt, ud_sock_t sock);

/**
 * Discard buffered packs from previous `ud_chck()'. */
extern int ud_dscrd(ud_sock_t sock);

#endif	/* UD_NEW_API */


#if !defined UD_NEW_API || defined UD_COMPAT
/* old api */
/**
 * Flags. */
typedef long unsigned int ud_flags_t;


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
/**
 * Callback function for subscriptions. */
typedef bool(*ud_subscr_f)(const ud_packet_t, ud_const_sockaddr_t, void *clo);

/**
 * Struct to handle conversations. */
struct ud_handle_s {
	/** Conversation number. */
	ud_convo_t convo:8;
	/** Socket. */
	int sock:24;
	ud_pktchn_t pktchn;
	/* our connexion later on */
	union ud_sockaddr_u sa;
	/* moving average roundtrip time (in nano seconds) */
	int mart;
	/* system score */
	int score;
};


/* connexion stuff */
/**
 * Initialise a handle into what's behind HDL. */
extern void init_unserding_handle(ud_handle_t hdl, int pref_fam, bool negop);
/**
 * Free a handle and all resources. */
extern void free_unserding_handle(ud_handle_t);
/**
 * Send packet PKT through the handle HDL.
 * Block until there is an answer. */
extern ssize_t ud_send_raw(ud_handle_t hdl, ud_packet_t pkt);
/**
 * Send a CMD-packet through the handle HDL. */
extern ud_convo_t ud_send_simple(ud_handle_t hdl, ud_pkt_cmd_t cmd);
/**
 * Wait (read block) until packets arrive or TIMEOUT millisecs have passed.
 * If packets were found this routine returns exactly one packet and
 * their content is copied into PKT. */
extern ssize_t
ud_recv_raw(ud_handle_t hdl, ud_packet_t pkt, int timeout);
/**
 * Like ud_recv_raw() but only receive packets that belong to convo CNO. */
extern void
ud_recv_convo(ud_handle_t hdl, ud_packet_t *pkt, int timeout, ud_convo_t cno);

/**
 * Subscribe to all traffic.
 * For each packet on the network call CB with the packet's buffer in PKT
 * and a user closure CLO.  After TIMEOUT milliseconds the callback is
 * called with an empty PACKET.  The function CB should return `true' if
 * it wishes to continue receiving packets, and should return `false' if
 * the subscription is annulled. */
extern void
ud_subscr_raw(ud_handle_t hdl, int timeout, ud_subscr_f cb, void *clo);

/**
 * Send packet to channel. */
extern ssize_t ud_chan_send(ud_chan_t, ud_packet_t);

/* inlines */
/**
 * Return the current conversation id of HDL. */
static inline ud_convo_t __attribute__((always_inline))
ud_handle_convo(ud_handle_t hdl)
{
	return hdl->convo;
}	

/**
 * Return the connexion socket stored inside HDL. */
static inline int __attribute__((always_inline))
ud_handle_sock(ud_handle_t hdl)
{
	return hdl->sock;
}	

/* lower level connexion stuff */
/**
 * Get a socket to communicate with service PORT. */
extern ud_chan_t ud_chan_init(short unsigned int port);
/**
 * Free resources associated with a connection. */
extern void ud_chan_fini(ud_chan_t);
#endif	/* !UD_NEW_API || UD_COMPAT */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_unserding_h_ */
