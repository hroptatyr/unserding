/*** mcast4.c -- ipv4 multicast handlers
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
#define SA_STRUCT		struct sockaddr_in6
#include "unserding.h"
#include "unserding-private.h"

#define MCAST_TIMEOUT		60
#define UDP_MULTICAST_TTL	16

static int lsock __attribute__((used));
static ev_io __srv_watcher __attribute__((aligned(16)));
static struct ip_mreq mreq4;
static struct ipv6_mreq mreq6;


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

static int
_mcast_listener_try(volatile struct addrinfo *lres)
{
	volatile int s;
	int retval;
	int one = 1;
	unsigned char ttl = UDP_MULTICAST_TTL;

	s = socket(lres->ai_family, SOCK_DGRAM, 0);
	if (s < 0) {
		UD_CRITICAL_MCAST("socket() failed, whysoever\n");
		return s;
	}
	__reuse_sock(s);

	/* we used to retry upon failure, but who cares */
	retval = bind(s, lres->ai_addr, lres->ai_addrlen);
	if (UNLIKELY(retval == -1)) {
		UD_CRITICAL_MCAST("bind() failed, whysoever\n");
		close(s);
		return -1;
	} else {
		UD_DEBUG_MCAST("listening to udp://[%s]"
			       ":" UD_NETWORK_SERVICE "\n",
			       lres->ai_canonname);
	}

	/* set up the multicast group and join it */
	inet_pton(AF_INET, UD_MCAST4_ADDR, &mreq4.imr_multiaddr.s_addr);
	mreq4.imr_interface.s_addr = htonl(INADDR_ANY);
	/* now truly join */
	if (UNLIKELY(setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
				&mreq4, sizeof(mreq4)) < 0)) {
		UD_DEBUG_MCAST("could not join the multicast group\n");
	} else {
		UD_DEBUG_MCAST("listening to udp://"
			       UD_MCAST4_ADDR ":" UD_NETWORK_SERVICE "\n");
	}

	/* set up the multi6cast group and join it */
	inet_pton(AF_INET6, UD_MCAST6_ADDR, &mreq6.ipv6mr_multiaddr.s6_addr);
	mreq6.ipv6mr_interface = 0;
	/* now truly join */
	if (UNLIKELY(setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP,
				&mreq6, sizeof(mreq6)) < 0)) {
		UD_DEBUG_MCAST("could not join the multi6cast group\n");
	} else {
		UD_DEBUG_MCAST("listening to udp://"
			       "[" UD_MCAST6_ADDR "]" ":"
			       UD_NETWORK_SERVICE "\n");
	}

	/* turn into a mcast sock and set a TTL */
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &one, sizeof(one));
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl));
	/* return the socket we've got */
	return s;
}

#if defined HAVE_GETADDRINFO
static int
mcast_listener_init(void)
{
	struct addrinfo *res;
	const struct addrinfo hints = {
		/* allow both v6 and v4 */
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = IPPROTO_UDP,
		/* specify to whom we listen */
		.ai_flags = AI_PASSIVE | AI_V4MAPPED | AI_ALL | AI_NUMERICHOST,
		/* naught out the rest */
		.ai_canonname = NULL,
		.ai_addr = NULL,
		.ai_next = NULL,
	};
	int retval;
	volatile int s;

	retval = getaddrinfo(NULL, UD_NETWORK_SERVICE, &hints, &res);
	if (retval != 0) {
		/* abort(); */
		return -1;
	}
	for (struct addrinfo *lres = res; lres; lres = lres->ai_next) {
		if ((s = _mcast_listener_try(lres)) >= 0) {
			UD_DEBUG_MCAST(":sock %d  :proto %d :host %s\n",
					s, lres->ai_protocol,
					lres->ai_canonname);
			break;
		}
	}

	freeaddrinfo(res);
	/* succeeded if > 0 */
	return s;
}
#else  /* !GETADDRINFO */
# error "Listen bloke, need getaddrinfo() bad, give me one or I'll stab myself."
#endif

static void
mcast_listener_deinit(int sock)
{
	/* linger the sink sock */
	__linger_sock(sock);
	UD_DEBUG_MCAST("closing listening socket %d...\n", sock);
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return;
}


/* this callback is called when data is writable on one of the polled socks */
#if 0
static void __attribute__((unused))
mcast_traf_wcb(EV_P_ ev_io *w, int revents)
{
	conn_ctx_t ctx = ev_wio_ctx(w);
	outbuf_t obuf;

	UD_DEBUG_MCAST("writing buffer to %d\n", ctx->snk);
	lock_obring(&ctx->obring);
	/* obtain the current output buffer */
	obuf = curr_outbuf(&ctx->obring);

	if (LIKELY(obuf->obufidx < obuf->obuflen)) {
		const char *buf =
			(char*)((long int)obuf->obuf & ~1UL) + obuf->obufidx;
		size_t blen = obuf->obuflen - obuf->obufidx;
		/* the actual write */
		obuf->obufidx += sendto(
			ctx->snk, buf, blen, 0,
			(struct sockaddr*)&ctx->sa, sizeof(ctx->sa));
		UD_DEBUG_MCAST("sent %ld\n", obuf->obufidx);
	}
	/* it's likely that we can output all at once */
	if (LIKELY(obuf->obufidx >= obuf->obuflen)) {
		/* free the buffer */
		free_outbuf(obuf);
		/* wind to the next outbuf */
		ctx->obring.curr_idx = step_obring_idx(ctx->obring.curr_idx);

		if (LIKELY(outbuf_free_p(curr_outbuf(&ctx->obring)))) {
			/* if nothing's to be printed just turn it off */
			ev_io_stop(EV_A_ w);
		}
	}
	unlock_obring(&ctx->obring);
	return;
}
#endif	/* 0 */

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
	job_t j = obtain_job(glob_jq);
	socklen_t lsa = sizeof(j->sa);

	UD_DEBUG_MCAST("incoming connection\n");
	nread = recvfrom(j->sock = w->fd, j->buf, JOB_BUF_SIZE, 0,
			 &j->sa, &lsa);
	/* obtain the address in human readable form */
	a = inet_ntop(j->sa.sin6_family,
		      j->sa.sin6_family == PF_INET6
		      ? (void*)&j->sa.sin6_addr
		      : (void*)&((struct sockaddr_in*)&j->sa)->sin_addr,
		      buf, sizeof(buf));
	p = ntohs(j->sa.sin6_port);
	UD_DEBUG_MCAST("Server: connect from host %s, port %d.\n", a, p);

	/* handle the reading */
	if (UNLIKELY(nread <= 0)) {
		UD_CRITICAL_MCAST("could not handle incoming connection\n");
		/* is that wise? */
		mcast_listener_deinit(w->fd);

	} else {
		j->blen = nread;
		j->workf = ud_parse;
		j->prntf = ud_print_mcast4;

		/* enqueue t3h job and copy the input buffer over to
		 * the job's work space */
		enqueue_job(glob_jq, j);
		/* now notify the slaves */
		trigger_job_queue();
	}
	return;
}


void
ud_print_mcast4(EV_P_ job_t j)
{
	ssize_t nwrit;

	/* the actual write */
	nwrit = sendto(j->sock, j->buf, j->blen, 0,
		       (struct sockaddr*)&j->sa, sizeof(j->sa));
	trigger_evloop(EV_A);
	return;
}

int
ud_attach_mcast4(EV_P)
{
	ev_io *srv_watcher = &__srv_watcher;

	/* get us a global sock */
	lsock = mcast_listener_init();

	if (LIKELY(lsock >= 0)) {
		/* initialise an io watcher, then start it */
		ev_io_init(srv_watcher, mcast_inco_cb, lsock, EV_READ);
		ev_io_start(EV_A_ srv_watcher);
	}
	return 0;
}

int
ud_detach_mcast4(EV_P)
{
	if (LIKELY(lsock >= 0)) {
		/* drop multicast group membership */
		setsockopt(lsock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
			   &mreq4, sizeof(mreq4));
		/* drop mcast6 group membership */
		setsockopt(lsock, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
			   &mreq6, sizeof(mreq6));
		/* and kick the socket */
		mcast_listener_deinit(lsock);
	}
	return 0;
}

/* mcast4.c ends here */
