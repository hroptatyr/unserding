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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define UD_NETWORK_SERVICE	8653
#define UD_NETWORK_SERVSTR	"8653"
/* 239.0.0.0/8 are organisational solicited v4 mcast addrs */
#define UD_MCAST4_ADDR		"239.86.53.1"
#define UD_MCAST4S2S_ADDR	"239.86.53.3"
/* ff3x::8000:0-ff3x::ffff:ffff - dynamically allocated by hosts when needed */
#define UD_MCAST6_ADDR		"ff38::8653:1"
#define UD_MCAST6S2S_ADDR	"ff38::8653:3"

/* our grand unified sockaddr thingie */
typedef union ud_sockaddr_u ud_sockaddr_t;

union ud_sockaddr_u {
		struct sockaddr_storage sas;
		struct sockaddr sa;
		struct sockaddr_in sa4;
		struct sockaddr_in6 sa6;
};

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
typedef bool(*ud_subscr_f)(const ud_packet_t pkt, void *clo);

/**
 * Struct to handle conversations. */
struct ud_handle_s {
	/** Conversation number. */
	ud_convo_t convo;
	/** Socket. */
	int sock:24;
	int epfd;
	ud_pktchn_t pktchn;
	/* our connexion later on */
	ud_sockaddr_t sa;
	/* our epoll event, very rudely opaquified */
	void *data[2];
	/* moving average roundtrip time (in nano seconds) */
	int mart;
	/* system score */
	int score;
};


#if defined __INTEL_COMPILER
#pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */
/* sockaddr stuff */
static inline short unsigned int __attribute__((always_inline))
ud_sockaddr_fam(const ud_sockaddr_t *sa)
{
	return sa->sa.sa_family;
}

static inline short unsigned int __attribute__((always_inline))
ud_sockaddr_4port(const ud_sockaddr_t *sa)
{
	return ntohs(sa->sa4.sin_port);
}

static inline short unsigned int __attribute__((always_inline))
ud_sockaddr_6port(const ud_sockaddr_t *sa)
{
	return ntohs(sa->sa6.sin6_port);
}

static inline short unsigned int __attribute__((always_inline))
ud_sockaddr_port(const ud_sockaddr_t *sa)
{
	/* should be properly switched? */
	return ntohs(sa->sa6.sin6_port);
}

static inline void __attribute__((always_inline))
ud_sockaddr_set_6port(ud_sockaddr_t *sa, uint16_t port)
{
	sa->sa6.sin6_port = htons(port);
	return;
}

static inline void __attribute__((always_inline))
ud_sockaddr_set_4port(ud_sockaddr_t *sa, uint16_t port)
{
	sa->sa4.sin_port = htons(port);
	return;
}

static inline void __attribute__((always_inline))
ud_sockaddr_set_port(ud_sockaddr_t *sa, uint16_t port)
{
	/* should be properly switched? */
	sa->sa6.sin6_port = htons(port);
	return;
}


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
extern void ud_send_raw(ud_handle_t hdl, ud_packet_t pkt);
/**
 * Send a CMD-packet through the handle HDL. */
extern ud_convo_t ud_send_simple(ud_handle_t hdl, ud_pkt_cmd_t cmd);
/**
 * Wait (read block) until packets arrive or TIMEOUT millisecs have passed.
 * If packets were found this routine returns exactly one packet and
 * their content is copied into PKT. */
extern void
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

static inline const void __attribute__((always_inline))*
ud_sockaddr_4addr(const ud_sockaddr_t *sa)
{
	return &sa->sa4.sin_addr;
}

static inline const void __attribute__((always_inline))*
ud_sockaddr_6addr(const ud_sockaddr_t *sa)
{
	return &sa->sa6.sin6_addr;
}

static inline const void __attribute__((always_inline))*
ud_sockaddr_addr(const ud_sockaddr_t *sa)
{
	switch (ud_sockaddr_fam(sa)) {
	case AF_INET6:
		return ud_sockaddr_6addr(sa);
	case AF_INET:
		return ud_sockaddr_4addr(sa);
	default:
		return NULL;
	}
}

static inline void
ud_handle_set_port(ud_handle_t hdl, uint16_t port)
{
	ud_sockaddr_set_port(&hdl->sa, port);
	return;
}

static inline void
ud_handle_set_6svc(ud_handle_t hdl)
{
	hdl->sa.sa6.sin6_family = AF_INET6;
	inet_pton(AF_INET6, UD_MCAST6_ADDR, &hdl->sa.sa6.sin6_addr);
	return;
}

static inline void
ud_handle_set_4svc(ud_handle_t hdl)
{
	hdl->sa.sa.sa_family = AF_INET;
	inet_pton(AF_INET, UD_MCAST4_ADDR, &hdl->sa.sa4.sin_addr);
	return;
}


/* specific services */
/**
 * Service 0004:
 * Ping/pong service to determine neighbours. */
#define UD_SVC_PING	0x0004

#endif	/* INCLUDED_unserding_h_ */
