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
/* because of splice() */
#define _GNU_SOURCE
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
#include "epoll-helpers.h"

#if defined DEBUG_FLAG
# include <assert.h>
#else
# define assert(args...)
#endif

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

static inline void
set_dccp_service(int s, int service)
{
	(void)setsockopt_int(s, SOL_DCCP, DCCP_SOCKOPT_SERVICE, service);
	return;
}

static inline void
ud_sockaddr_set_port(ud_sockaddr_t sa, uint16_t port)
{
	sa->sa6.sin6_port = htons(port);
	return;
}

/* pseudo constructor, singleton */
static ep_ctx_t
epoll_guts(void)
{
	static struct ep_ctx_s gepg = FLEX_EP_CTX_INITIALISER(4);

	if (UNLIKELY(gepg.sock == -1)) {
		init_ep_ctx(&gepg, gepg.nev);
	}
	return &gepg;
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
	/* turn off nagle'ing of data */
	setsock_nodelay(s);
	/* create our global epoll guts, maybe malloc me and put me in a hdl */
	(void)epoll_guts();
	return s;
}

int
dccp_listen(int s, uint16_t port)
{
	ud_sockaddr_u sa;

	if (s < 0) {
		return s;
	}
	memset(&sa, 0, sizeof(sa));
	sa.sa6.sin6_family = AF_INET6;
	sa.sa6.sin6_addr = in6addr_any;
	sa.sa6.sin6_port = htons(port);
	/* bind the socket to the local address and port. salocal is sockaddr
	 * of local IP and port */
	if (bind(s, &sa.sa, sizeof(sa)) < 0) {
		return -1;
	}
	/* listen on that port for incoming connections */
	return listen(s, MAX_DCCP_CONNECTION_BACK_LOG);
}

int
dccp_accept(int s, int timeout)
{
	ud_sockaddr_u remo_sa;
	socklen_t remo_sa_len;
	ep_ctx_t epg = epoll_guts();
	int res;

	/* otherwise bother our epoll structure */
	ep_prep_reader(epg, s);
	if ((res = ep_wait(epg, timeout)) == 0) {
		res = -1;
	} else if (res < 0 || res & (EPOLLERR | EPOLLHUP)) {
		res = -1;
	} else {
		/* everything went fine, invariant res == 1, accept connections */
		memset(&remo_sa, 0, sizeof(remo_sa));
		remo_sa_len = sizeof(remo_sa);
		res = accept(s, &remo_sa.sa, &remo_sa_len);
	}
	ep_fini(epg, s);
	return res;
}

int
dccp_connect(int s, ud_sockaddr_u host, uint16_t port, int timeout)
{
	int res = 0;
	ep_ctx_t epg = epoll_guts();

	ud_sockaddr_set_port(&host, port);

	/* prepare, connect and wait for the magic to happen */
	ep_prep_writer(epg, s);
	/* wait for writability */
	if ((res = ep_wait(epg, timeout)) <= 0) {
		/* means we've timed out */
		res = -1;
		goto out;
	}

	/* oh, we're writable, good, just issue the connect() */
	if ((res = connect(s, &host.sa, sizeof(host))) == 0) {
		/* oh, nifty, connect worked right away */
		goto out;
	}
	/* otherwise connect ought to return EINPROGRESS */
	assert(errno == EINPROGRESS);

	/* let's see */
	if ((res = ep_wait(epg, timeout)) <= 0) {
		/* means we've timed out */
		res = -1;
		goto out;

	} else if (res & (EPOLLERR | EPOLLHUP)) {
		res = -1;
		goto out;

	} else {
		/* ah, must be our connect */
		;
	}
out:
	ep_fini(epg, s);
	return res;
}

void
dccp_close(int s)
{
	/* how about we let this guy live on forever? */
#if 0
	free_epoll_guts(gepg);
#endif
	close(s);
	return;
}

#define ESUCCESS	0

ssize_t
dccp_send(int s, const char *buf, size_t bsz)
{
	ep_ctx_t epg = epoll_guts();
	ssize_t res = 0;

	/* add s to the list of observed sockets */
	ep_prep_writer(epg, s);
	if ((res = ep_wait(epg, 5000)) > 0) {
		if ((res = send(s, buf, bsz, 0)) == -1) {
			perror("send error: ");
		}
	}
	ep_fini(epg, s);
	return res;
}

ssize_t
dccp_recv(int s, char *restrict buf, size_t bsz)
{
	ep_ctx_t epg = epoll_guts();
	ssize_t res = 0;

	/* add s to the list of observed sockets */
	ep_prep_reader(epg, s);
	if ((res = ep_wait(epg, 5000)) > 0) {
		res = recv(s, buf, bsz, 0);
	}
	ep_fini(epg, s);
	return res;
}

ssize_t
dccp_splice(int in_s, int out_s)
{
	ep_ctx_t epg = epoll_guts();
	ssize_t res = 0;
	loff_t off = 0;

	/* add s to the list of observed sockets */
	ep_prep_reader(epg, in_s);
	if ((res = ep_wait(epg, 5000)) > 0) {
		res = splice(in_s, NULL, out_s, &off, 4096, 0);
	}
	ep_fini(epg, in_s);
	return res;
}

/* dccp.c ends here */
