/*** tcp.c -- ipv6 tcp handlers
 *
 * Copyright (C) 2009 Sebastian Freundt
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif	/* HAVE_SYS_SOCKET_H */
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif	/* HAVE_NETINET_IN_H */
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif	/* HAVE_ARPA_INET_H */
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
/* our main goodness */
#include "unserding-dbg.h"
#include "unserding-nifty.h"
#include "mcast.h"
#include "tcp.h"
#include "ud-sock.h"

struct epguts_s {
	struct epoll_event ev[1];
	int sock;
};

/* sure? */
static struct epguts_s gepg[1] = {{.sock = -1}};

static void
setsock_nonblock(int sock)
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

/* epoll guts */
static void
init_epoll_guts(struct epguts_s *epg)
{
	struct epoll_event *ev = epg->ev;

	if (epg->sock != -1) {
		return;
	}

	/* obtain an epoll handle and make it non-blocking*/
#if 0
/* too new, needs >= 2.6.30 */
	epg->sock = epoll_create1(0);
#else
	epg->sock = epoll_create(1);
#endif
	setsock_nonblock(epg->sock);

	/* register for input, oob, error and hangups */
	ev->events = EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
	/* register our data */
	ev->data.ptr = epg;
	return;
}

static void
free_epoll_guts(struct epguts_s *epg)
{
	if (LIKELY(epg->sock >= 0)) {
		/* close the epoll socket */
		close(epg->sock);
		/* wipe */
		epg->sock = -1;
	}
	return;
}

static void
ud_ep_prep(struct epguts_s *epg, int s, int addflags)
{
	int epfd = epg->sock;
	struct epoll_event ev = *epg->ev;

	/* add additional ADDFLAGS */
	ev.events |= addflags;
	/* add S to the epoll descriptor EPFD */
	(void)epoll_ctl(epfd, EPOLL_CTL_ADD, s, &ev);
	return;
}

static int
ud_ep_wait(struct epguts_s *epg, int timeout)
{
	struct epoll_event ev[1];
	int epfd = epg->sock, res;
	/* wait and return */
	res = epoll_wait(epfd, ev, 1, timeout);
	return res == 1 ? ev->events : res;
}

static void
ud_ep_fini(struct epguts_s *epg, int s)
{
	int epfd = epg->sock;
	struct epoll_event *ev = epg->ev;

	/* remove S from the epoll descriptor EPFD */
	(void)epoll_ctl(epfd, EPOLL_CTL_DEL, s, ev);
	return;
}


int
tcp_open(void)
{
	int s;

	if ((s = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		return s;
	}
	/* mark the address as reusable */
	setsock_reuseaddr(s);
	/* make socket non blocking */
	setsock_nonblock(s);
	/* create our global epoll guts, maybe malloc me and put me in a hdl */
	init_epoll_guts(gepg);
	return s;
}

int
tcp_accept(int s, uint16_t port, int timeout)
{
	ud_sockaddr_u sa;
	ud_sockaddr_u remo_sa;
	socklen_t remo_sa_len;
	int res = 0;
	struct epguts_s *epg = gepg;

	if (s < 0) {
		return s;
	}
	memset(&sa, 0, sizeof(sa));
	sa.sa6.sin6_family = AF_INET6;
	sa.sa6.sin6_addr = in6addr_any;
	sa.sa6.sin6_port = htons(port);
	/* bind the socket to the local address and port. salocal is sockaddr
	 * of local IP and port */
	res |= bind(s, &sa.sa, sizeof(sa));
	/* listen on that port for incoming connections */
	res |= listen(s, 2);
	if (res != 0) {
		return res;
	}
	/* otherwise bother our epoll structure */
	ud_ep_prep(epg, s, EPOLLIN);
	if ((res = ud_ep_wait(epg, timeout)) <= 0) {
		res = -1;
		fprintf(stderr, "tcp accept() timed out\n");
	} else if ((res & EPOLLERR) && (res & EPOLLHUP)) {
		fprintf(stderr, "epoll error %x\n", res);
		res = -1;
	} else {
		/* accept connections */
		memset(&remo_sa, 0, sizeof(remo_sa));
		remo_sa_len = sizeof(remo_sa);
		res = accept(s, &remo_sa.sa, &remo_sa_len);
	}
	ud_ep_fini(epg, s);
	return res;
}

int
tcp_connect(int s, ud_sockaddr_u host, uint16_t port, int timeout)
{
	int res = 0;
	struct epguts_s *epg = gepg;

	host.sa4.sin_port = htons(port);
	/* turn off nagle'ing of data */
	setsock_nodelay(s);

	errno = 0;
	if ((res = connect(s, &host.sa, sizeof(host))) == 0) {
		/* oh, nifty, connect worked right away */
		return res;
	}

	/* prepare, connect and wait for the magic to happen */
	ud_ep_prep(epg, s, EPOLLOUT);

	/* let's see */
	if ((res = ud_ep_wait(epg, timeout)) <= 0) {
		/* means we've timed out */
		res = -1;
		fprintf(stderr, "tcp connect() timed out\n");
	} else if ((res & EPOLLERR) && (res & EPOLLHUP)) {
		fprintf(stderr, "epoll error %x\n", res);
		res = -1;
	} else {
		/* ah, must be our connect */
#if 0
		errno = 0;
		res = connect(s, &host.sa, sizeof(host));
#endif
	}
	ud_ep_fini(epg, s);
	return res;
}

void
tcp_close(int s)
{
	free_epoll_guts(gepg);
	close(s);
	return;
}

#define ESUCCESS	0

ssize_t
tcp_send(int s, const char *buf, size_t bsz)
{
	struct epguts_s *epg = gepg;
	ssize_t res = 0;

	/* add s to the list of observed sockets */
	ud_ep_prep(epg, s, EPOLLOUT);
	if ((res = ud_ep_wait(epg, 1000)) > 0) {
		res = send(s, buf, bsz, 0);
	}
	ud_ep_fini(epg, s);
	return res;
}

ssize_t
tcp_recv(int s, char *restrict buf, size_t bsz)
{
	struct epguts_s *epg = gepg;
	ssize_t res = 0;

	/* add s to the list of observed sockets */
	ud_ep_prep(epg, s, EPOLLIN);
	if ((res = ud_ep_wait(epg, 1000)) > 0) {
		res = recv(s, buf, bsz, 0);
	}
	ud_ep_fini(epg, s);
	return res;
}

/* tcp.c ends here */
