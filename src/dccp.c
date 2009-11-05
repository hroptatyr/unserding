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
/* our main goodness */
#include "unserding.h"
#include "unserding-ctx.h"
#include "unserding-dbg.h"
#include "unserding-nifty.h"

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

static uint32_t cnt = 0;

static void
__req_dccp(void)
{
	ud_sockaddr_u sa, remo_sa;
	size_t remo_sa_len;
	int s;
	/* turn off bind address checking, and allow port numbers to be reused -
	 * otherwise the TIME_WAIT phenomenon will prevent binding to these
	 * address.port combinations for (2 * MSL) seconds. */
	int on = 1;
	int res;

	s = socket(PF_INET6, SOCK_DCCP, IPPROTO_DCCP);
	cnt++;
	setsockopt(s, SOL_DCCP, SO_REUSEADDR, &on, sizeof(on));
	setsockopt(s, SOL_DCCP, DCCP_SOCKOPT_SERVICE, &cnt, sizeof(cnt));
	/* make a timeout for the accept call below */
	on = 10000 /* milliseconds */;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &on, sizeof(on));

	memset(&sa, 0, sizeof(sa));
	sa.sa6.sin6_family = AF_INET6;
	sa.sa6.sin6_addr = in6addr_any;
	sa.sa6.sin6_port = htons(UD_NETWORK_SERVICE);
	/* bind the socket to the local address and port. salocal is sockaddr
	 * of local IP and port */
	res = bind(s, &sa.sa, sizeof(sa));
	/* listen on that port for incoming connections */
	res = listen(s, MAX_DCCP_CONNECTION_BACK_LOG);

	res = accept(s, &remo_sa.sa, &remo_sa_len);
	UD_DEBUG("finally %d\n", res);
	close(res);
	close(s);
	return;
}


/* jobs */
static void
req_dccp(job_t UNUSED(j))
{
	__req_dccp();
	return;
}


void
init_dccp(void *UNUSED(clo))
{
	ud_set_service(0x000a, req_dccp, NULL);
	return;
}

void
deinit_dccp(void *UNUSED(clo))
{
	ud_set_service(0x000a, req_dccp, NULL);
	return;
}

/* dccp.c ends here */
