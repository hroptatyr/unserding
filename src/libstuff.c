/*** libstuff.c -- unserding library definitions
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
/* conditionalise on me */
#include <sys/epoll.h>

/* our master include */
#define SA_STRUCT		struct sockaddr_in6
#include "unserding.h"
#include "protocore.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined countof
# define countof(_x)	(sizeof(_x) / sizeof(*_x))
#endif

/**
 * Timeout in seconds. */
#define UD_SENDRECV_TIMEOUT	10
#define UDP_MULTICAST_TTL	16
#define SOCK_INVALID		(int)0xffffff

/* server to client goodness */
static struct sockaddr_in6 __sa6;
static struct sockaddr_in __sa4;


static int
mcast_init(void)
{
	volatile int s;

	if ((s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		return SOCK_INVALID;
	}
	/* prepare the __sa6 structure, but do not join */
	__sa6.sin6_family = PF_INET6;
	inet_pton(AF_INET6, UD_MCAST6_ADDR, &__sa6.sin6_addr);
	__sa6.sin6_port = htons(UD_NETWORK_SERVICE);
	/* prepare the __sa4 structure, but do not join */
	__sa4.sin_family = PF_INET;
	inet_pton(AF_INET, UD_MCAST4_ADDR, &__sa4.sin_addr);
	__sa4.sin_port = htons(UD_NETWORK_SERVICE);
	return s;
}

static inline int __attribute__((always_inline, gnu_inline))
ud_handle_epfd(ud_handle_t hdl)
{
	if (LIKELY(hdl->epfd >= 0)) {
		return hdl->epfd;
	} else {
		return hdl->epfd = epoll_create(1);
	}
}


/* public funs */
void
ud_send_raw(ud_handle_t hdl, ud_packet_t pkt)
{
	int s = ud_handle_sock(hdl);

	/* always send to the mcast addresses */
	(void)sendto(s, pkt.pbuf, pkt.plen, 0,
		     (struct sockaddr*)&__sa4, sizeof(struct sockaddr_in));
	/* ship to m6cast addr */
	(void)sendto(s, pkt.pbuf, pkt.plen, 0,
		     (struct sockaddr*)&__sa6, sizeof(struct sockaddr_in6));
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
	int s = ud_handle_sock(hdl);
	int epfd = ud_handle_epfd(hdl);
	int nfds;
	ssize_t nread;
	struct epoll_event ev, *events = NULL;
	char buf[UDPC_SIMPLE_PKTLEN];
	ud_packet_t tmp = {.plen = countof(buf), .pbuf = buf};

	if (UNLIKELY(pkt->plen == 0)) {
		return;
	}

	/* register for input, oob, error and hangups */
	ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
	/* register our data */
	ev.data.ptr = hdl;
	/* add S to the epoll descriptor EPFD */
	(void)epoll_ctl(epfd, EPOLL_CTL_ADD, s, &ev);
	/* now wait */
	do {
		nfds = epoll_wait(epfd, events, 1, to);
		/* no need to loop atm, nfds can be 0 or 1 */
		if (UNLIKELY(nfds == 0)) {
			/* nothing received */
			pkt->plen = 0;
			goto out;
		}
		/* otherwise NFDS was 1 and it MUST be our socket */
		nread = recvfrom(s, buf, countof(buf), 0, NULL, 0);
	} while (!udpc_pkt_for_us_p(tmp, cno));
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
init_unserding_handle(ud_handle_t hdl)
{
	hdl->convo = 0;
	hdl->sock = mcast_init();
	hdl->epfd = -1;
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
