/*** libstuff.c -- unserding library definitions
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
/* conditionalise on me */
#include <sys/epoll.h>
/* conditionalise on me too */
#include <fcntl.h>

/* our master include */
#include "unserding.h"
#include "protocore.h"
#include "protocore-private.h"

#include "svc-pong.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined countof
# define countof(_x)	(sizeof(_x) / sizeof(*_x))
#endif

#if defined DEBUG_FLAG
# define UD_DEBUG_SENDRECV(args...)		\
	fprintf(stderr, "[libunserding/sendrecv] " args)
/* we could define the PKTTRAF debug flag but it's seriously slow */
#else  /* !DEBUG_FLAG */
# define UD_DEBUG_SENDRECV(args...)
# undef DEBUG_PKTTRAF_FLAG
#endif	/* DEBUG_FLAG */

/**
 * Timeout in milliseconds. */
#define UD_SENDRECV_TIMEOUT	10
#define UDP_MULTICAST_TTL	16
#define SOCK_INVALID		(int)0xffffff


static void
__set_nonblck(int sock)
{
	int opts;

	/* get former options */
	opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		return;
	}
	opts |= O_NONBLOCK;
	(void)fcntl(sock, F_SETFL, opts);
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
mcast6_init(ud_handle_t hdl)
{
#if defined IPPROTO_IPV6
	volatile int s;
	int opt = 0;

	/* try v6 first */
	if ((hdl->sock = s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		return SOCK_INVALID;
	}

#if defined IPV6_V6ONLY
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif	/* IPV6_V6ONLY */

	fiddle_with_mtu(s);
	ud_handle_set_6svc(hdl);
	ud_handle_set_port(hdl, UD_NETWORK_SERVICE);
	return s;

#else  /* !IPPROTO_IPV6 */

	return SOCK_INVALID;
#endif	/* IPPROTO_IPV6 */
}

static int
mcast4_init(ud_handle_t hdl)
{
	volatile int s;

	/* try v6 first */
	if ((s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		return SOCK_INVALID;
	}

	ud_handle_set_4svc(hdl);
	ud_handle_set_port(hdl, UD_NETWORK_SERVICE);
	return s;
}

static inline int
ud_handle_epfd(ud_handle_t hdl)
{
	return hdl->epfd;
}

static inline struct epoll_event*
ud_handle_epev(ud_handle_t hdl)
{
	return (struct epoll_event*)&hdl->data;
}

typedef struct __convo_flt_s {
	ud_packet_t *pkt;
	ud_convo_t cno;
} *__convo_flt_t;

static bool
__pkt_our_convo_p(const ud_packet_t pkt, void *clo)
{
	__convo_flt_t flt = clo;
	if (pkt.plen == 0) {
		return false;
	} else if (udpc_pkt_cno(pkt) == flt->cno) {
		size_t sz = pkt.plen < flt->pkt->plen
			? pkt.plen
			: flt->pkt->plen;
		memcpy(flt->pkt->pbuf, pkt.pbuf, sz);
		return false;
	}
	return true;
}

/* epoll guts */
static void
init_epoll_guts(ud_handle_t hdl)
{
	struct epoll_event *ev = ud_handle_epev(hdl);

	/* obtain an epoll handle and make it non-blocking*/
#if 0
/* too new, needs >= 2.6.30 */
	hdl->epfd = epoll_create1(0);
#else
	hdl->epfd = epoll_create(1);
#endif
	__set_nonblck(hdl->epfd);

	/* register for input, oob, error and hangups */
	ev->events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
	/* register our data */
	ev->data.ptr = hdl;
	return;
}

static void
free_epoll_guts(ud_handle_t hdl)
{
	if (LIKELY(hdl->epfd >= 0)) {
		/* close the epoll socket */
		close(hdl->epfd);
		/* wipe */
		hdl->epfd = -1;
		/* ah well */
		hdl->data[0] = 0;
		hdl->data[1] = 0;
	}
	return;
}

static void
ud_ep_prep(ud_handle_t hdl)
{
	int s = ud_handle_sock(hdl);
	int epfd = ud_handle_epfd(hdl);
	struct epoll_event *ev = ud_handle_epev(hdl);

	/* add S to the epoll descriptor EPFD */
	(void)epoll_ctl(epfd, EPOLL_CTL_ADD, s, ev);
	return;
}

static int
ud_ep_wait(ud_handle_t hdl, int timeout)
{
	int epfd = ud_handle_epfd(hdl);
	struct epoll_event *events = NULL;
	/* wait and return */
	return epoll_wait(epfd, events, 1, timeout);
}

static void
ud_ep_fini(ud_handle_t hdl)
{
	int s = ud_handle_sock(hdl);
	int epfd = ud_handle_epfd(hdl);
	struct epoll_event *ev = ud_handle_epev(hdl);

	/* remove S from the epoll descriptor EPFD */
	(void)epoll_ctl(epfd, EPOLL_CTL_DEL, s, ev);
	return;
}

static int
fiddle_with_timeout(ud_handle_t hdl, int timeout)
{
	if (timeout > 0) {
		return timeout;
	}
	return hdl->mart;
}


/* public funs */
void
ud_send_raw(ud_handle_t hdl, ud_packet_t pkt)
{
	int s = ud_handle_sock(hdl);

	if (sizeof(hdl->sa) != sizeof(hdl->sa.sas)) {
		abort();
	}

	/* always send to the mcast addresses */
	(void)sendto(s, pkt.pbuf, pkt.plen, 0, &hdl->sa.sa, sizeof(hdl->sa));
	return;
}

void
ud_recv_raw(ud_handle_t hdl, ud_packet_t pkt, int timeout)
{
	int s = ud_handle_sock(hdl);
	int nfds;
	ssize_t nread;
	char buf[UDPC_PKTLEN];

	if (UNLIKELY(pkt.plen == 0)) {
		return;
	}

	/* wait for events */
	ud_ep_prep(hdl);
	timeout = fiddle_with_timeout(hdl, timeout);
	nfds = ud_ep_wait(hdl, timeout);
	/* no need to loop atm, nfds can be 0 or 1 */
	if (UNLIKELY(nfds == 0)) {
		/* nothing received */
		pkt.plen = 0;
		goto out;
	}
	/* otherwise NFDS was 1 and it MUST be our socket */
	nread = recvfrom(s, buf, countof(buf), 0, NULL, 0);
#if defined DEBUG_PKTTRAF_FLAG
	udpc_print_pkt(BUF_PACKET(buf));
#endif	/* DEBUG_PKTTRAF_FLAG */
out:
	ud_ep_fini(hdl);
	return;
}

void
ud_recv_convo(ud_handle_t hdl, ud_packet_t *pkt, int to, ud_convo_t cno)
{
	struct __convo_flt_s flt = {.pkt = pkt, .cno = cno};
	ud_subscr_raw(hdl, to, __pkt_our_convo_p, &flt);
	return;
}

/* subscriptions, basically receivers with callbacks */
void
ud_subscr_raw(ud_handle_t hdl, int timeout, ud_subscr_f cb, void *clo)
{
	int s = ud_handle_sock(hdl);
	int nfds;
	ssize_t nread;
	static __thread char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);

	/* wait for events */
	ud_ep_prep(hdl);
	timeout = fiddle_with_timeout(hdl, timeout);
	do {
		nfds = ud_ep_wait(hdl, timeout);
		/* no need to loop atm, nfds can be 0 or 1 */
		if (UNLIKELY(nfds == 0)) {
			/* nothing received */
			memset(buf, 0, sizeof(buf));
			pkt.plen = 0;
		} else {
			/* otherwise NFDS was 1 and it MUST be our socket */
			nread = recvfrom(s, buf, sizeof(buf), 0, NULL, 0);
			pkt.plen = nread;
		}
	} while ((*cb)(pkt, clo));
	ud_ep_fini(hdl);
	return;
}


/* protocol funs */
ud_convo_t
ud_send_simple(ud_handle_t hdl, ud_pkt_cmd_t cmd)
{
	ud_convo_t cno = ud_handle_convo(hdl);
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);

	udpc_make_pkt(pkt, cno, /*pno*/0, cmd);
	ud_send_raw(hdl, pkt);
	hdl->convo++;
	return cno;
}


void
init_unserding_handle(ud_handle_t hdl, int pref_fam, bool negop)
{
	hdl->convo = 0;
	hdl->epfd = -1;

	switch (pref_fam) {
	default:
	case PF_UNSPEC:
		if (mcast6_init(hdl) != SOCK_INVALID) {
			break;
		}
	case PF_INET:
		hdl->sock = mcast4_init(hdl);
		break;
	case PF_INET6:
		(void)mcast6_init(hdl);
		break;
	}
	/* operate in non-blocking mode */
	__set_nonblck(hdl->sock);
	/* initialise the epoll backend */
	init_epoll_guts(hdl);
	if (negop) {
		/* fiddle with the mart slot */
		hdl->mart = UD_SENDRECV_TIMEOUT;
		ud_svc_nego_score(hdl, UD_SENDRECV_TIMEOUT);
	} else {
		hdl->mart = UD_SENDRECV_TIMEOUT;
	}
	return;
}

void
free_unserding_handle(ud_handle_t hdl)
{
	int s = ud_handle_sock(hdl);

	if (LIKELY(s != SOCK_INVALID)) {
		/* and kick the socket */
		shutdown(s, SHUT_RDWR);
		close(s);
		hdl->sock = SOCK_INVALID;
	}
	/* also free the epoll cruft */
	free_epoll_guts(hdl);
	return;
}

/* libstuff.c ends here */
