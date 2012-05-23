/*** mcast.h -- ipv6 multicast handlers
 *
 * Copyright (C) 2008-2012 Sebastian Freundt
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

#if !defined INCLUDED_mcast_h_
#define INCLUDED_mcast_h_

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif /* __cplusplus */

#define UD_NETWORK_SERVICE	8653
#define UD_NETWORK_SERVSTR	"8653"
/* http://www.iana.org/assignments/ipv6-multicast-addresses/ lists us 
 * as ff0x:0:0:0:0:0:0:134 */
/* node-local */
#define UD_MCAST6_NODE_LOCAL	"ff01::134"
/* link-local */
#define UD_MCAST6_LINK_LOCAL	"ff02::134"
/* site-local */
#define UD_MCAST6_SITE_LOCAL	"ff05::134"

/* just offer a default address for some tools */
#define UD_MCAST6_ADDR		UD_MCAST6_LINK_LOCAL

/* our grand unified sockaddr thingie */
typedef union ud_sockaddr_u ud_sockaddr_u;
typedef union ud_sockaddr_u *ud_sockaddr_t;
typedef const union ud_sockaddr_u *ud_const_sockaddr_t;

union ud_sockaddr_u {
		struct sockaddr_storage sas;
		struct sockaddr sa;
		struct sockaddr_in6 sa6;
};

/**
 * Structure that just consists of socket and destination. */
typedef const struct ud_chan_s *ud_chan_t;

/**
 * Lower-level structure to handle convos. */
struct ud_chan_s {
	int sock;
	ud_sockaddr_u sa;
};


#if defined __INTEL_COMPILER
#pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */
/* sockaddr stuff */
static inline short unsigned int
ud_sockaddr_fam(ud_const_sockaddr_t sa)
{
	return sa->sa.sa_family;
}

static inline short unsigned int
ud_sockaddr_port(ud_const_sockaddr_t sa)
{
	return ntohs(sa->sa6.sin6_port);
}

static inline const void*
ud_sockaddr_addr(ud_const_sockaddr_t sa)
{
	return &sa->sa6.sin6_addr;
}

static inline void
ud_sockaddr_ntop(char *restrict buf, size_t len, ud_const_sockaddr_t sa)
{
	short unsigned int fam = ud_sockaddr_fam(sa);
	const void *saa = ud_sockaddr_addr(sa);
	(void)inet_ntop(fam, saa, buf, len);
	return;
}


/* public offerings */
/**
 * Subscribe to service PORT in the unserding network.
 * Return a socket that can be select()'d or poll()'d. */
extern int ud_mcast_init(short unsigned int port);
/**
 * Unsubscribe from socket SOCK. */
extern void ud_mcast_fini(int sock);

/**
 * Turn looping of packets on or off.
 * Packets to the socket itself will be repeated. */
extern int ud_mcast_loop(int s, int on);

/**
 * Subscribe to services on CHAN in the unserding network.
 * Return a socket that can be select()'d or poll()'d. */
extern int ud_chan_init_mcast(ud_chan_t chan);
/**
 * Unsubscribe from multicast on CHAN. */
extern void ud_chan_fini_mcast(ud_chan_t chan);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_mcast_h_ */
