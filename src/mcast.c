/*** mcast4.c -- ipv4, ipv6 multicast handlers
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
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
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif
/* our master include */
#if defined AF_INET6
# define SA_STRUCT		struct sockaddr_in6
#else  /* !AF_INET6 */
# define SA_STRUCT		struct sockaddr_in
#endif	/* AF_INET6 */
#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"
#include "protocore-private.h"

/**
 * Rate used for keep-alive pings to neighbours in seconds. */
#define S2S_BRAG_RATE		10
/**
 * Negotiation timeout in seconds. */
#define S2S_NEGO_TIMEOUT	2
#define UDP_MULTICAST_TTL	16

#if !defined HAVE_GETADDRINFO
# error "Listen bloke, need getaddrinfo() bad, give me one or I'll stab myself."
#endif

static int lsock4 __attribute__((used));
static int lsock6 __attribute__((used));

static ev_timer ALGN16(__s2s_watcher);

/* v4 stuff */
static ev_io ALGN16(__srv4_watcher);
static struct ip_mreq ALGN16(mreq4);
/* server to client goodness */
static ud_sockaddr_t __sa4 = {
	.sa4.sin_addr = {0}
};

/* dual-stack v6 stuff */
#if defined IPPROTO_IPV6
static ev_io ALGN16(__srv6_watcher);
static struct ipv6_mreq ALGN16(mreq6);
/* server to client goodness */
static ud_sockaddr_t __sa6 = {
	.sa6.sin6_addr = IN6ADDR_ANY_INIT
};
#endif	/* AF_INET */


/* socket goodies */
static inline void
__reuse_sock(int sock)
{
	const int one = 1;

#if defined SO_REUSEADDR
	UD_DEBUG_MCAST("setting option SO_REUSEADDR for sock %d\n", sock);
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
		UD_CRITICAL_MCAST("setsockopt(SO_REUSEADDR) failed\n");
	}
#else
# error "Go away!"
#endif
#if defined SO_REUSEPORT
	UD_DEBUG_MCAST("setting option SO_REUSEPORT for sock %d\n", sock);
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) < 0) {
		UD_CRITICAL_MCAST("setsockopt(SO_REUSEPORT) failed\n");
	}
#endif
	return;
}

static inline void
__linger_sock(int sock)
{
#if defined SO_LINGER
	struct linger lng;

	lng.l_onoff = 1;	/* 1 == on */
	lng.l_linger = 1;	/* linger time in seconds */

	UD_DEBUG_MCAST("setting option SO_LINGER for sock %d\n", sock);
	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &lng, sizeof(lng)) < 0) {
		UD_CRITICAL_MCAST("setsockopt(SO_LINGER) failed\n");
	}
#else
# error "Go away"
#endif
	return;
}

static void
__mcast4_join_group(int s, const char *addr, struct ip_mreq *mreq)
{
	int UNUSED(one) = 1, UNUSED(zero) = 0;

#if defined IP_PKTINFO && 0
	/* turn on packet info, very linux-ish!!! */
	setsockopt(s, IPPROTO_IPV6, IP_PKTINFO, &one, sizeof(one)) ;
#endif
#if defined IP_RECVSTDADDR && 0
	/* turn on destination addr */
	setsockopt(s, IPPROTO_IPV6, IP_RECVSTDADDR, &one, sizeof(one)) ;
#endif
	/* turn off loopback */
	setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, &zero, sizeof(zero));

	/* set up the multicast group and join it */
	inet_pton(AF_INET, addr, &mreq->imr_multiaddr.s_addr);
	mreq->imr_interface.s_addr = htonl(INADDR_ANY);
	/* now truly join */
	if (UNLIKELY(setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
				mreq, sizeof(*mreq)) < 0)) {
		UD_DEBUG_MCAST("could not join the multicast group\n");
	} else {
		UD_DEBUG_MCAST("port %d listening to udp://%s:0\n", s, addr);
	}
	return;
}

#if defined IPPROTO_IPV6
static void
__mcast6_join_group(int s, const char *addr, struct ipv6_mreq *mreq)
{
	int opt = 0;
	unsigned char ttl = UDP_MULTICAST_TTL;

	/* turn into a mcast sock and set a TTL */
	opt = 0;
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &opt, sizeof(opt));
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl));

	/* set up the multicast group and join it */
	inet_pton(AF_INET6, addr, &mreq->ipv6mr_multiaddr.s6_addr);
	mreq->ipv6mr_interface = 0;

	/* now truly join */
	if (UNLIKELY(setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP,
				mreq, sizeof(*mreq)) < 0)) {
		UD_DEBUG_MCAST("could not join the multi6cast group\n");
	} else {
		UD_DEBUG_MCAST("port %d listening to udp://[%s]"
			       ":" UD_NETWORK_SERVSTR "\n", s, addr);
	}
	return;
}
#endif	/* IPPROTO_IPV6 */

static void
__mcast4_leave_group(int s, struct ip_mreq *mreq)
{
	/* drop multicast group membership */
	setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, mreq, sizeof(*mreq));
	return;
}

#if defined IPPROTO_IPV6
static void
__mcast6_leave_group(int s, struct ipv6_mreq *mreq)
{
	/* drop mcast6 group membership */
	setsockopt(s, IPPROTO_IPV6, IPV6_LEAVE_GROUP, mreq, sizeof(*mreq));
	return;
}
#endif	/* IPPROTO_IPV6 */

static int
mcast4_listener_init(void)
{
	int retval;
	volatile int s;

	__sa4.sa4.sin_family = AF_INET;
	__sa4.sa4.sin_port = htons(UD_NETWORK_SERVICE);

	if (LIKELY((s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) >= 0)) {
		/* likely case upfront */
		;
	} else {
		UD_DEBUG_MCAST("socket() failed ... I'm clueless now\n");
		return s;
	}
	/* allow many many many servers on that port */
	__reuse_sock(s);

	/* we used to retry upon failure, but who cares */
	retval = bind(s, (struct sockaddr*)&__sa4, sizeof(__sa4));
	if (retval == -1) {
		UD_DEBUG_MCAST("bind() failed %d %d\n", errno, EINVAL);
		close(s);
		return -1;
	}

	/* join the mcast group */
	__mcast4_join_group(s, UD_MCAST4_ADDR, &mreq4);
	/* endow our s2c and s2s structs */
	__sa4.sa4.sin_addr = mreq4.imr_multiaddr;

	/* return the socket we've got */
	/* succeeded if > 0 */
	return s;
}

static int
mcast6_listener_init(void)
{
#if defined IPPROTO_IPV6
	int retval;
	volatile int s;

	__sa6.sa6.sin6_family = AF_INET6;
	__sa6.sa6.sin6_port = htons(UD_NETWORK_SERVICE);

	if (LIKELY((s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) >= 0)) {
		/* likely case upfront */
		;
	} else {
		UD_DEBUG_MCAST("socket() failed ... I'm clueless now\n");
		return s;
	}
	/* allow many many many servers on that port */
	__reuse_sock(s);

#if defined IPV6_V6ONLY
	retval = 1;
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

	/* we used to retry upon failure, but who cares */
	retval = bind(s, (struct sockaddr*)&__sa6, sizeof(__sa6));

	if (retval == -1) {
		UD_DEBUG_MCAST("bind() failed %d %d\n", errno, EINVAL);
		close(s);
		return -1;
	}

	/* join the mcast group */
	__mcast6_join_group(s, UD_MCAST6_ADDR, &mreq6);
	/* endow our s2c and s2s structs */
	__sa6.sa6.sin6_addr = mreq6.ipv6mr_multiaddr;

	/* return the socket we've got */
	/* succeeded if > 0 */
	return s;

#else  /* !IPPROTO_IPV6 */
	return -1;
#endif	/* IPPROTO_IPV6 */
}

static void
mcast_listener_deinit(int sock)
{
	/* drop multicast group membership */
	__mcast4_leave_group(sock, &mreq4);
#if defined IPPROTO_IPV6
	__mcast6_leave_group(sock, &mreq6);
#endif	/* IPPROTO_IPV6 */
	/* linger the sink sock */
	__linger_sock(sock);
	UD_DEBUG_MCAST("closing listening socket %d...\n", sock);
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return;
}


static char scratch_buf[UDPC_PKTLEN];

/* this callback is called when data is readable on the main server socket */
static void
mcast_inco_cb(EV_P_ ev_io *w, int revents)
{
	ssize_t nread;
	char buf[INET6_ADDRSTRLEN];
	/* the address in human readable form */
	const char *a;
	/* the port (in host-byte order) */
	uint16_t p;
	/* a job */
	job_t j;
	socklen_t lsa = sizeof(j->sa);

	UD_DEBUG_MCAST("incoming connexion\n");
	if (UNLIKELY((j = make_job()) == NULL)) {
		UD_CRITICAL("no job slots ... leaping\n");
		/* just read the packet off of the wire */
		(void)recv(w->fd, scratch_buf, UDPC_PKTLEN, 0);
		trigger_job_queue();
		return;
	}

	j->sock = w->fd;
	nread = recvfrom(w->fd, j->buf, JOB_BUF_SIZE, 0, &j->sa.sa, &lsa);
	/* obtain the address in human readable form */
	a = inet_ntop(ud_sockaddr_fam(&j->sa),
		      ud_sockaddr_addr(&j->sa), buf, sizeof(buf));
	p = ud_sockaddr_port(&j->sa);
	UD_DEBUG_MCAST("sock %d connect from host %s port %d\n", w->fd, a, p);
	UD_LOG_MCAST(":sock %d connect :from [%s]:%d\n"
		     "                                         "
		     ":len %04x :cno %02x :pno %06x :cmd %04x :mag %04x\n",
		     w->fd, a, p,
		     (unsigned int)nread,
		     udpc_pkt_cno(JOB_PACKET(j)),
		     udpc_pkt_pno(JOB_PACKET(j)),
		     udpc_pkt_cmd(JOB_PACKET(j)),
		     ntohs(((const uint16_t*)j->buf)[3]));

	/* handle the reading */
	if (UNLIKELY(nread <= 0)) {
		UD_CRITICAL_MCAST("could not handle incoming connection\n");
		return;
	}

	j->blen = nread;

	/* spit the packet in its raw shape */
	ud_fprint_pkt_raw(JOB_PACKET(j), logout);

	/* enqueue t3h job and copy the input buffer over to
	 * the job's work space */
	enqueue_job(glob_jq, j);
	/* now notify the slaves */
	trigger_job_queue();
	return;
}


void
send_cl(job_t j)
{
	/* prepare */
	if (UNLIKELY(j->blen == 0)) {
		return;
	}
	/* write back to whoever sent the packet */
	(void)sendto(j->sock, j->buf, j->blen, 0, &j->sa.sa, sizeof(j->sa));
	usleep(10);
	return;
}

void
send_m4(job_t j)
{
	/* prepare */
	if (UNLIKELY(j->blen == 0)) {
		return;
	}
	/* send to the m4cast address */
	(void)sendto(lsock4, j->buf, j->blen, 0, &__sa4.sa, sizeof(__sa4.sa4));
	return;
}

void __attribute__((unused))
send_m6(job_t j)
{
	/* prepare */
	if (UNLIKELY(j->blen == 0)) {
		return;
	}
	/* send to the m6cast address */
	(void)sendto(lsock6, j->buf, j->blen, 0, &__sa6.sa, sizeof(__sa6.sa6));
	return;
}

void __attribute__((unused))
send_m46(job_t j)
{
	/* prepare */
	if (UNLIKELY(j->blen == 0)) {
		return;
	}
	/* always send to the mcast addresses */
	(void)sendto(lsock4, j->buf, j->blen, 0, &__sa4.sa, sizeof(__sa4.sa4));
	/* ship to m6cast addr */
	(void)sendto(lsock6, j->buf, j->blen, 0, &__sa6.sa, sizeof(__sa6.sa6));
	return;
}


int
ud_attach_mcast(EV_P)
{
	/* get us a global sock */
	lsock4 = mcast4_listener_init();
	lsock6 = mcast6_listener_init();

	if (UNLIKELY(lsock4 < 0 && lsock6 < 0)) {
		return -1;
	}
	if (LIKELY(lsock4 >= 0)) {
		ev_io *srv_watcher = &__srv4_watcher;
		/* initialise an io watcher, then start it */
		ev_io_init(srv_watcher, mcast_inco_cb, lsock4, EV_READ);
		ev_io_start(EV_A_ srv_watcher);
	}
	if (LIKELY(lsock6 >= 0)) {
		ev_io *srv_watcher = &__srv6_watcher;
		/* initialise an io watcher, then start it */
		ev_io_init(srv_watcher, mcast_inco_cb, lsock6, EV_READ);
		ev_io_start(EV_A_ srv_watcher);
	}
	return 0;
}

int
ud_detach_mcast(EV_P)
{
	ev_io *srv_watcher;
	ev_timer *s2s_watcher = &__s2s_watcher;

	/* close the sockets before we stop the watchers */
	if (LIKELY(lsock4 >= 0)) {
		/* and kick the socket */
		mcast_listener_deinit(lsock4);
	}
	if (LIKELY(lsock6 >= 0)) {
		/* and kick the socket */
		mcast_listener_deinit(lsock6);
	}

	/* stop the guy that watches the socket */
	srv_watcher = &__srv4_watcher;
	ev_io_stop(EV_A_ srv_watcher);
	srv_watcher = &__srv6_watcher;
	ev_io_stop(EV_A_ srv_watcher);

	/* stop the timer */
	ev_timer_stop(EV_A_ s2s_watcher);
	return 0;
}

/* mcast.c ends here */
