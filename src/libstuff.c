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
#define SA_STRUCT		struct sockaddr_in6
#include "unserding.h"
#include "protocore.h"
#include "protocore-private.h"

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
#else  /* !DEBUG_FLAG */
# define UD_DEBUG_SENDRECV(args...)
#endif	/* DEBUG_FLAG */

/**
 * Timeout in seconds. */
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

static int
mcast6_init(ud_handle_t hdl)
{
#if defined IPPROTO_IPV6
	volatile int s;

	/* try v6 first */
	if ((s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		return SOCK_INVALID;
	}

#if defined IPV6_V6ONLY
	{
		int one = 1;
		setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
	}
#endif	/* IPV6_V6ONLY */

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

static inline int __attribute__((always_inline, gnu_inline))
ud_handle_epfd(ud_handle_t hdl)
{
	if (UNLIKELY(hdl->epfd < 0)) {
		hdl->epfd = epoll_create(4);
		/* set non-blocking */
		(void)fcntl(hdl->epfd, F_SETFL, O_NONBLOCK);
	}
	return hdl->epfd;
}

static bool
__pkt_our_convo_p(const ud_packet_t pkt, void *clo)
{
	ud_convo_t cno = (long unsigned int)clo;
	return udpc_pkt_cno(pkt) == cno;
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
	int epfd = ud_handle_epfd(hdl);
	int nfds;
	ssize_t nread;
	struct epoll_event ev, *events = NULL;
	char buf[UDPC_SIMPLE_PKTLEN];
	ud_packet_t tmp = {.plen = countof(buf), .pbuf = buf};

	if (UNLIKELY(pkt.plen == 0)) {
		return;
	}

	/* register for input, oob, error and hangups */
	ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
	/* register our data */
	ev.data.ptr = hdl;
	/* add S to the epoll descriptor EPFD */
	(void)epoll_ctl(epfd, EPOLL_CTL_ADD, s, &ev);
	/* now wait */
	nfds = epoll_wait(epfd, events, 1, timeout);
	/* no need to loop atm, nfds can be 0 or 1 */
	if (UNLIKELY(nfds == 0)) {
		/* nothing received */
		pkt.plen = 0;
		goto out;
	}
	/* otherwise NFDS was 1 and it MUST be our socket */
	nread = recvfrom(s, buf, countof(buf), 0, NULL, 0);
	udpc_print_pkt(tmp);
out:
	/* remove S from the epoll descriptor EPFD */
	(void)epoll_ctl(epfd, EPOLL_CTL_DEL, s, &ev);
	return;
}

void
ud_recv_convo(ud_handle_t hdl, ud_packet_t *pkt, int to, ud_convo_t cno)
{
	void *clo = (void*)(long unsigned int)cno;
	ud_recv_pred(hdl, pkt, to, __pkt_our_convo_p, clo);
	return;
}

#define MAX_EVENTS	4
static struct epoll_event evbuf[MAX_EVENTS];

void
ud_recv_pred(ud_handle_t hdl, ud_packet_t *pkt, int to, ud_pred_f pf, void *clo)
{
	int s = ud_handle_sock(hdl);
	int epfd = ud_handle_epfd(hdl);
	int nfds;
	ssize_t nread;
	struct epoll_event ev, *events = evbuf;
	char buf[UDPC_SIMPLE_PKTLEN];
	ud_packet_t tmp = {.plen = countof(buf), .pbuf = buf};

	/* register for input, oob, error and hangups */
	ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLET;
	/* register our data */
	ev.data.ptr = hdl;
	/* add S to the epoll descriptor EPFD */
	(void)epoll_ctl(epfd, EPOLL_CTL_ADD, s, &ev);
	/* now wait */
wait:
	memset(events, 0, sizeof(evbuf));
	nfds = epoll_wait(epfd, events, MAX_EVENTS, to);
	UD_DEBUG_SENDRECV("received %d events on socket %d\n", nfds, s);

	/* no need to loop atm, nfds can be 0 or 1 */
	if (UNLIKELY(nfds == 0 || nfds == -1)) {
		/* nothing received */
		pkt->plen = 0;
		goto out;
	}
	/* otherwise NFDS was 1 and it MUST be our socket */
	do {
		if ((nread = recv(s, buf, countof(buf), 0)) < 0) {
			/* batshit! start over
			 * we could assert(errno == EAGAIN); */
			UD_DEBUG_SENDRECV("read failed ... starting over\n");
			goto wait;
		}
		UD_DEBUG_SENDRECV("read %ld bytes\n", (long int)nread);
		tmp.plen = nread;
#if defined DEBUG_FLAG
		ud_fprint_pkt_raw(tmp, stderr);
#endif	/* DEBUG_FLAG */
	} while (!udpc_pkt_valid_p(tmp) || !pf(tmp, clo));

	if (LIKELY(nread > 0)) {
		if (LIKELY((size_t)nread < pkt->plen)) {
			pkt->plen = nread;
		}
		memcpy(pkt->pbuf, buf, pkt->plen);
	} else {
		pkt->plen = 0;
	}
out:
	/* remove S from the epoll descriptor EPFD */
	(void)epoll_ctl(epfd, EPOLL_CTL_DEL, s, &ev);
	return;
}


/* protocol funs */
ud_convo_t
ud_send_simple(ud_handle_t hdl, ud_pkt_cmd_t cmd)
{
	ud_convo_t cno = ud_handle_convo(hdl);
	char buf[UDPC_SIMPLE_PKTLEN];
	ud_packet_t pkt = {.plen = countof(buf), .pbuf = buf};

	udpc_make_pkt(pkt, cno, /*pno*/0, cmd);
	ud_send_raw(hdl, pkt);
	hdl->convo++;
	return cno;
}


void
init_unserding_handle(ud_handle_t hdl, int pref_fam)
{
	hdl->convo = 0;
	hdl->epfd = -1;

	switch (pref_fam) {
	default:
	case PF_UNSPEC:
		if ((hdl->sock = mcast6_init(hdl)) != SOCK_INVALID) {
			break;
		}
	case PF_INET:
		hdl->sock = mcast4_init(hdl);
		break;
	case PF_INET6:
		hdl->sock = mcast6_init(hdl);
		break;
	}
	/* operate in non-blocking mode */
	__set_nonblck(hdl->sock);
	return;
}

void
free_unserding_handle(ud_handle_t hdl)
{
	int s = ud_handle_sock(hdl);

	if (LIKELY(hdl->epfd >= 0)) {
		/* close the epoll socket */
		close(hdl->epfd);
		/* wipe */
		hdl->epfd = -1;
	}
	if (LIKELY(s != SOCK_INVALID)) {
		/* and kick the socket */
		shutdown(s, SHUT_RDWR);
		close(s);
		hdl->sock = SOCK_INVALID;
	}
	return;
}

/* libstuff.c ends here */
