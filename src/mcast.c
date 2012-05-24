/*** mcast.c -- ipv6 multicast handlers
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#if defined HAVE_NETDB_H
# include <netdb.h>
#elif defined HAVE_LWRES_NETDB_H
# include <lwres/netdb.h>
#endif	/* NETDB */
#include <errno.h>
/* our master include */
#if defined AF_INET6
# define SA_STRUCT		struct sockaddr_in6
#else  /* !AF_INET6 */
# define SA_STRUCT		struct sockaddr_in
#endif	/* AF_INET6 */
#include "unserding.h"
#include "unserding-private.h"
#include "mcast.h"
#include "protocore.h"
#include "protocore-private.h"
#include "ud-sock.h"
#include "unserding-dbg.h"

#if defined UNSERLIB
/* no output at all in lib mode */
# define UD_CRITICAL_MCAST(args...)
# define UD_INFO_MCAST(args...)
# define UD_DEBUG_MCAST(args...)
#elif defined DEBUG_FLAG
# define UD_CRITICAL_MCAST(args...)					\
	do {								\
		UD_LOGOUT("[unserding/input/mcast] CRITICAL " args);	\
		UD_SYSLOG(LOG_CRIT, "[input/mcast] CRITICAL " args);	\
	} while (0)
# define UD_DEBUG_MCAST(args...)					\
	do {								\
		UD_LOGOUT("[unserding/input/mcast] " args);		\
		UD_SYSLOG(LOG_INFO, "[input/mcast] " args);		\
	} while (0)
# define UD_INFO_MCAST(args...)						\
	do {								\
		UD_LOGOUT("[unserding/input/mcast] " args);		\
		UD_SYSLOG(LOG_INFO, "[input/mcast] " args);		\
	} while (0)
#else  /* !DEBUG_FLAG */
# define UD_CRITICAL_MCAST(args...)				\
	UD_SYSLOG(LOG_CRIT, "[input/mcast] CRITICAL " args)
# define UD_INFO_MCAST(args...)					\
	UD_SYSLOG(LOG_INFO, "[input/mcast] " args)
# define UD_DEBUG_MCAST(args...)
#endif	/* DEBUG_FLAG */

/**
 * Rate used for keep-alive pings to neighbours in seconds. */
#define S2S_BRAG_RATE		10
/**
 * Negotiation timeout in seconds. */
#define S2S_NEGO_TIMEOUT	2
#define UDP_MULTICAST_TTL	64

#if !defined HAVE_GETADDRINFO
# error "Listen bloke, need getaddrinfo() bad, give me one or I'll stab myself."
#endif

/* dual-stack v6 stuff */
#if defined IPPROTO_IPV6
/* node local, site local and link local */
static struct ipv6_mreq ALGN16(mreq6_nolo);
static struct ipv6_mreq ALGN16(mreq6_silo);
static struct ipv6_mreq ALGN16(mreq6_lilo);
#endif	/* AF_INET */


/* socket goodies */
static void
__reuse_sock(int sock)
{
	UD_DEBUG_MCAST("setting option SO_REUSEADDR for sock %d\n", sock);
	if (setsock_reuseaddr(sock) < 0) {
		UD_CRITICAL_MCAST("setsockopt(SO_REUSEADDR) failed\n");
	}
	UD_DEBUG_MCAST("setting option SO_REUSEPORT for sock %d\n", sock);
	if (setsock_reuseport(sock) < 0) {
		UD_CRITICAL_MCAST("setsockopt(SO_REUSEPORT) failed\n");
	}
	return;
}

static void
__linger_sock(int sock)
{
	UD_DEBUG_MCAST("setting option SO_LINGER for sock %d\n", sock);
	if (setsock_linger(sock, 1) < 0) {
		UD_CRITICAL_MCAST("setsockopt(SO_LINGER) failed\n");
	}
	return;
}

#if defined IPPROTO_IPV6
static int
__mcast6_join_group(int s, const char *addr, struct ipv6_mreq *r)
{
	/* set up the multicast group and join it */
	inet_pton(AF_INET6, addr, &r->ipv6mr_multiaddr.s6_addr);
	r->ipv6mr_interface = 0;

	/* now truly join */
	return setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, r, sizeof(*r));
}

static void
__mcast6_leave_group(int s, struct ipv6_mreq *mreq)
{
	/* drop mcast6 group membership */
	setsockopt(s, IPPROTO_IPV6, IPV6_LEAVE_GROUP, mreq, sizeof(*mreq));
	return;
}

static int
__mcast6_join(int s, short unsigned int UNUSED(port))
{
	struct {
		const char *a;
		struct ipv6_mreq *r;
	} g[] = {
		{UD_MCAST6_NODE_LOCAL, &mreq6_nolo},
		{UD_MCAST6_LINK_LOCAL, &mreq6_lilo},
		{UD_MCAST6_SITE_LOCAL, &mreq6_silo},
	};

	for (size_t i = 0; i < countof(g); i++) {
		UD_DEBUG_MCAST(
			"sock %d joining udp://[%s]:%hu\n", s, g[i].a, port);
		if (UNLIKELY(__mcast6_join_group(s, g[i].a, g[i].r) < 0)) {
			UD_DEBUG_MCAST("  could not join the group\n");
		}
	}
#if 0
	/* endow our s2c and s2s structs */
	__sa6.sa6.sin6_addr = mreq6.ipv6mr_multiaddr;
#endif	/* 0 */
	return 0;
}
#endif	/* IPPROTO_IPV6 */

static int
mcast6_listener_init(int s, short unsigned int port)
{
#if defined IPPROTO_IPV6
	int retval;
	int opt;
	ud_sockaddr_u __sa6 = {
		.sa6.sin6_addr = IN6ADDR_ANY_INIT
	};

	__sa6.sa6.sin6_family = AF_INET6;
	__sa6.sa6.sin6_port = htons(port);

	/* allow many many many servers on that port */
	__reuse_sock(s);

	/* turn multicast looping on */
	ud_mcast_loop(s, 1);
#if defined IPV6_V6ONLY
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &retval, sizeof(retval));
#endif	/* IPV6_V6ONLY */
#if defined IPV6_USE_MIN_MTU
	/* use minimal mtu */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU, &opt, sizeof(opt));
#endif
#if defined IPV6_DONTFRAG
	/* rather drop a packet than to fragment it */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_DONTFRAG, &opt, sizeof(opt));
#endif
#if defined IPV6_RECVPATHMTU
	/* obtain path mtu to send maximum non-fragmented packet */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_RECVPATHMTU, &opt, sizeof(opt));
#endif
#if defined IPV6_MULTICAST_HOPS
	opt = UDP_MULTICAST_TTL;
	/* turn into a mcast sock and set a TTL */
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &opt, sizeof(opt));
#endif	/* IPV6_MULTICAST_HOPS */

	/* we used to retry upon failure, but who cares */
	if ((retval = bind(s, (struct sockaddr*)&__sa6, sizeof(__sa6))) < 0) {
		UD_DEBUG_MCAST("bind() failed\n");
		return -1;
	}

	/* join the mcast group(s) */
	__mcast6_join(s, port);

	/* return the socket we've got */
	/* succeeded if > 0 */
	return 0;

#else  /* !IPPROTO_IPV6 */
	return -1;
#endif	/* IPPROTO_IPV6 */
}

static void
mcast_listener_deinit(int sock)
{
	/* drop multicast group membership */
#if defined IPPROTO_IPV6
	__mcast6_leave_group(sock, &mreq6_silo);
	__mcast6_leave_group(sock, &mreq6_lilo);
	__mcast6_leave_group(sock, &mreq6_nolo);
#endif	/* IPPROTO_IPV6 */
	/* linger the sink sock */
	__linger_sock(sock);
	return;
}

static inline void
ud_sockaddr_set_port(ud_sockaddr_t sa, uint16_t port)
{
	sa->sa6.sin6_port = htons(port);
	return;
}


static inline void
ud_chan_set_port(struct ud_chan_s *c, uint16_t port)
{
	ud_sockaddr_set_port(&c->sa, port);
	return;
}

static inline void
ud_chan_set_svc(struct ud_chan_s *c)
{
	c->sa.sa6.sin6_family = AF_INET6;
	/* we pick link-local here for simplicity */
	inet_pton(AF_INET6, UD_MCAST6_LINK_LOCAL, &c->sa.sa6.sin6_addr);
	/* set the flowinfo */
	c->sa.sa6.sin6_flowinfo = 0;
	return;
}

static void
fiddle_with_mtu(int __attribute__((unused)) s)
{
#if defined IPV6_PATHMTU
	struct ip6_mtuinfo mtui;
	socklen_t mtuilen = sizeof(mtui);
#endif	/* IPV6_PATHMTU */

#if defined IPV6_USE_MIN_MTU
	/* use minimal mtu */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU, &opt, sizeof(opt));
#endif
#if defined IPV6_DONTFRAG
	/* rather drop a packet than to fragment it */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_DONTFRAG, &opt, sizeof(opt));
#endif
#if defined IPV6_RECVPATHMTU
	/* obtain path mtu to send maximum non-fragmented packet */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_RECVPATHMTU, &opt, sizeof(opt));
#endif
#if defined IPV6_PATHMTU
	/* obtain current pmtu */
	if (getsockopt(s, IPPROTO_IPV6, IPV6_PATHMTU, &mtui, &mtuilen) < 0) {
		perror("could not obtain pmtu");
	} else {
		fprintf(stderr, "pmtu is %d\n", mtui.ip6m_mtu);
	}
#endif
	return;
}

static int
mcast6_init(void)
{
#if defined IPPROTO_IPV6
	volatile int s;
	int opt = 0;

	/* try v6 first */
	if ((s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		return -1;
	}

#if defined IPV6_V6ONLY
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif	/* IPV6_V6ONLY */

	fiddle_with_mtu(s);
	return s;

#else  /* !IPPROTO_IPV6 */
out:
	return -1;
#endif	/* IPPROTO_IPV6 */
}


/* public raw mcast socks */
int
ud_mcast_init(short unsigned int port)
{
	volatile int s;

	if (UNLIKELY((s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0)) {
		UD_DEBUG_MCAST("socket() failed ... I'm clueless now\n");
	} else if (mcast6_listener_init(s, port) < 0) {
		close(s);
	}
	return s;
}

void
ud_mcast_fini(int sock)
{
	mcast_listener_deinit(sock);
	UD_DEBUG_MCAST("closing listening socket %d...\n", sock);
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return;
}

int
ud_mcast_loop(int s, int on)
{
#if defined IPV6_MULTICAST_LOOP
	/* don't loop */
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(on));
#else  /* !IPV6_MULTICAST_LOOP */
# warning multicast looping cannot be turned on or off
#endif	/* IPV6_MULTICAST_LOOP */
	return on;
}


/* chan stuff */
int
ud_chan_init_mcast(ud_chan_t c)
{
	/* we need a modifiable object */
	union {
		ud_chan_t c;
		struct ud_chan_s *p;
	} u = {c};
	short unsigned int port = ud_sockaddr_port(&c->sa);
	volatile int s;

	if (UNLIKELY((s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0)) {
		UD_DEBUG_MCAST("socket() failed ... I'm clueless now\n");
	} else if (mcast6_listener_init(s, port) < 0) {
		return -1;
	}
	return u.p->mcfd = s;
}

void
ud_chan_fini_mcast(ud_chan_t c)
{
	/* we need a modifiable object */
	union {
		ud_chan_t c;
		struct ud_chan_s *p;
	} u = {c};

	if (c->mcfd > 0) {
		mcast_listener_deinit(c->sock);
		close(c->mcfd);
		u.p->mcfd = -1;
	}
	return;
}

ud_chan_t
ud_chan_init(short unsigned int port)
{
	struct ud_chan_s *res;

	res = calloc(1, sizeof(*res));
	if ((res->sock = mcast6_init()) > 0) {
		setsock_nonblock(res->sock);
		ud_chan_set_svc(res);
		ud_chan_set_port(res, port);
	}
	return res;
}

void
ud_chan_fini(ud_chan_t c)
{
	/* we need a modifiable object */
	union {
		ud_chan_t c;
		struct ud_chan_s *p;
	} u = {c};

	/* and kick the socket */
	if (c->mcfd > 0) {
		/* kick multicast listener */
		close(c->mcfd);
		u.p->mcfd = -1;
	}
	if (c->sock > 0) {
		/* kick channel socket */
		close(c->sock);
		u.p->sock = -1;
	}
	free(u.p);
	return;
}

ssize_t
ud_chan_send(ud_chan_t c, ud_packet_t pkt)
{
	/* always send to the mcast addresses */
	return sendto(c->sock, pkt.pbuf, pkt.plen, 0, &c->sa.sa, sizeof(c->sa));
}

/* mcast.c ends here */
