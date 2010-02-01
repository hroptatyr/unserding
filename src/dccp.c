/*** dccp.c -- ipv6 dccp handlers
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
#include "dccp.h"
#include "ud-sock.h"

#if !defined SOCK_DCCP
# define SOCK_DCCP		6
#endif	/* !SOCK_DCCP */
#if !defined IPPROTO_DCCP
# define IPPROTO_DCCP		33
#endif	/* !IPPROTO_DCCP */
#if !defined SOL_DCCP
# define SOL_DCCP		269
#endif	/* !SOL_DCCP */
#if !defined DCCP_SOCKOPT_SERVICE
# define DCCP_SOCKOPT_SERVICE	2
#endif	/* !DCCP_SOCKOPT_SERVICE */
#if !defined MAX_DCCP_CONNECTION_BACK_LOG
# define MAX_DCCP_CONNECTION_BACK_LOG	5
#endif	/* !MAX_DCCP_CONNECTION_BACK_LOG */
#if !defined DCCP_SOCKOPT_PACKET_SIZE
# define DCCP_SOCKOPT_PACKET_SIZE 1
#endif	/* !DCCP_SOCKOPT_PACKET_SIZE */

struct epguts_s {
	struct epoll_event ev[1];
	int sock;
};

/* sure? */
static struct epguts_s gepg[1] = {{.sock = -1}};

static inline void
set_dccp_service(int s, int service)
{
	(void)setsockopt_int(s, SOL_DCCP, DCCP_SOCKOPT_SERVICE, service);
	return;
}

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
	ev->events = EPOLLPRI | EPOLLERR | EPOLLHUP;
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
dccp_open(void)
{
	int s;

	if ((s = socket(PF_INET6, SOCK_DCCP, IPPROTO_DCCP)) < 0) {
		return s;
	}
	/* mark the address as reusable */
	setsock_reuseaddr(s);
	/* impose a sockopt service, we just use 1 for now */
	set_dccp_service(s, 1);
	/* make a timeout for the accept call below */
	setsock_rcvtimeo(s, 2000);
	/* make socket non blocking */
	setsock_nonblock(s);
	/* create our global epoll guts, maybe malloc me and put me in a hdl */
	init_epoll_guts(gepg);
	return s;
}

int
dccp_accept(int s, uint16_t port, int timeout)
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
	res |= listen(s, MAX_DCCP_CONNECTION_BACK_LOG);
	if (res != 0) {
		return res;
	}
	/* otherwise bother our epoll structure */
	ud_ep_prep(epg, s, EPOLLIN);
	if ((res = ud_ep_wait(epg, timeout)) <= 0) {
		res = -1;
		fprintf(stderr, "dccp accept() timed out\n");
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
dccp_connect(int s, ud_sockaddr_u host, uint16_t port, int timeout)
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
	/* otherwise assume res == -1 and errno == EINPROGRESS */
	if (errno != EINPROGRESS) {
		return res;
	}

	/* prepare, connect and wait for the magic to happen */
	ud_ep_prep(epg, s, EPOLLOUT);

	/* let's see */
	if ((res = ud_ep_wait(epg, timeout)) <= 0) {
		/* means we've timed out */
		res = -1;
		fprintf(stderr, "dccp connect() timed out\n");
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
dccp_close(int s)
{
	free_epoll_guts(gepg);
	close(s);
	return;
}

#define ESUCCESS	0

ssize_t
dccp_send(int s, const char *buf, size_t bsz)
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
dccp_recv(int s, char *restrict buf, size_t bsz)
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

/* dccp.c ends here */
