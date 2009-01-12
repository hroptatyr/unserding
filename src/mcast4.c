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
#include "protocore.h"

#define S2S_BRAG_RATE		10
#define UDP_MULTICAST_TTL	16

#if !defined HAVE_GETADDRINFO
# error "Listen bloke, need getaddrinfo() bad, give me one or I'll stab myself."
#endif

static int lsock __attribute__((used));
static ud_sysid_t myid = 0;
static ev_io ALGN16(__srv_watcher);
static ev_timer ALGN16(__s2s_watcher);
static struct ip_mreq ALGN16(mreq4);
static struct ipv6_mreq ALGN16(mreq6);
/* server to client goodness */
static struct sockaddr_in6 __sa6;
static struct sockaddr_in __sa4;

#define MAXHOSTNAMELEN		64
/* text section */
size_t s2s_oi_len;
static char s2s_oi[MAXHOSTNAMELEN] = "hy ";


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
		UD_DEBUG_MCAST("port %d listening to udp://%s"
			       ":" UD_NETWORK_SERVICE "\n", s, addr);
	}
	return;
}

static void
__mcast6_join_group(int s, const char *addr, struct ipv6_mreq *mreq)
{
	int UNUSED(one) = 1, UNUSED(zero) = 0;
	unsigned char ttl = UDP_MULTICAST_TTL;

#if defined IPV6_PKTINFO && 0
	/* turn on packet info, very linux-ish!!! */
	setsockopt(s, IPPROTO_IPV6, IPV6_PKTINFO, &one, sizeof(one));
#endif
#if defined IPV6_RECVPKTINFO && 0
	/* turn on destination addr */
	setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO, &one, sizeof(one));
#endif
	/* turn into a mcast sock and set a TTL */
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &zero, sizeof(zero));
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
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
			       ":" UD_NETWORK_SERVICE "\n", s, addr);
	}
	return;
}

static void
__mcast4_leave_group(int s, struct ip_mreq *mreq)
{
	/* drop multicast group membership */
	setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, mreq, sizeof(*mreq));
	return;
}

static void
__mcast6_leave_group(int s, struct ipv6_mreq *mreq)
{
	/* drop mcast6 group membership */
	setsockopt(s, IPPROTO_IPV6, IPV6_LEAVE_GROUP, mreq, sizeof(*mreq));
	return;
}

static int
mcast_listener_try(volatile struct addrinfo *lres)
{
	volatile int s;
	int retval;

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
	return s;
}

static int
mcast46_listener_init(void)
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
		if ((s = mcast_listener_try(lres)) >= 0) {
			/* join the mcast group */
			__mcast6_join_group(s, UD_MCAST6_ADDR, &mreq6);
			/* endow our s2c and s2s structs */
			memcpy(&__sa6, lres->ai_addr, sizeof(__sa6));
			__sa6.sin6_addr = mreq6.ipv6mr_multiaddr;
			/* join the mcast group */
			__mcast4_join_group(s, UD_MCAST4_ADDR, &mreq4);
			/* endow our s2c and s2s structs */
			memcpy(&__sa4, lres->ai_addr, sizeof(__sa4));
			__sa4.sin_addr = mreq4.imr_multiaddr;

			UD_DEBUG_MCAST(":sock %d  :proto %d :host %s\n",
				       s, lres->ai_protocol,
				       lres->ai_canonname);
			break;
		}
	}

	freeaddrinfo(res);
	/* return the socket we've got */
	/* succeeded if > 0 */
	return s;
}

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
	UD_DEBUG_MCAST("sock %d connect from host %s port %d\n", w->fd, a, p);

	/* handle the reading */
	if (UNLIKELY(nread <= 0)) {
		UD_CRITICAL_MCAST("could not handle incoming connection\n");
		/* is that wise? */
		mcast_listener_deinit(w->fd);
		return;
	} else if (LIKELY(!udpc_pkt_for_us_p(j->buf, myid))) {
		UD_DEBUG_MCAST("pkt not for us, burying him\n");
		return;
	}

	j->blen = nread;
	j->workf = ud_proto_parse;
	j->prntf = ud_print_mcast4;

	/* enqueue t3h job and copy the input buffer over to
	 * the job's work space */
	enqueue_job(glob_jq, j);
	/* now notify the slaves */
	trigger_job_queue();
	return;
}

#if 0
/* later */
static void
mcast_inco_cb2(EV_P_ ev_io *w, int revents)
{
	ssize_t nread;
	char buf[INET6_ADDRSTRLEN];
	/* the address in human readable form */
	const char *a;
	/* the port (in host-byte order) */
	uint16_t p;
	/* a job */
	job_t j = obtain_job(glob_jq);
	struct msghdr msg;
	struct iovec iov;
	struct sockaddr_in receiver_addr, sender_addr;
	char ctrl[2 * sizeof(struct cmsghdr) +
		  sizeof(struct in_addr) +
		  sizeof(struct in_pktinfo)];
	struct in_pktinfo pktinfo;
	struct sockaddr_in myaddr;

	msg.msg_name = &j->sa;
	msg.msg_namelen = sizeof(j->sa);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_iov->iov_base = j->buf;
	msg.msg_iov->iov_len = JOB_BUF_SIZE;
	msg.msg_control = ctrl;
	msg.msg_controllen = countof(ctrl);
	msg.msg_flags = 0;

	UD_DEBUG_MCAST("incoming connection\n");
	nread = recvmsg(j->sock = w->fd, &msg, 0);

	/* obtain the address in human readable form */
	a = inet_ntop(j->sa.sin6_family,
		      j->sa.sin6_family == PF_INET6
		      ? (void*)&j->sa.sin6_addr
		      : (void*)&((struct sockaddr_in*)&j->sa)->sin_addr,
		      buf, sizeof(buf));
	p = ntohs(j->sa.sin6_port);
	UD_DEBUG_MCAST("sock %d connect from host %s port %d\n", w->fd, a, p);

#if 0
	for (struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg); cmptr;
	     cmptr = CMSG_NXTHDR(&msg, cmptr)) {

#ifdef IP_RECVSTDADDR
		if ( cmptr->cmsg_level == IPPROTO_IP &&
		     cmptr->cmsg_type == IP_RECVDSTADDR ) {
			memcpy(&myaddr.sin_addr, CMSG_DATA(cmptr),
			       sizeof(struct in_addr));
		}
#endif
#ifdef IP_PKTINFO
		if ( cmptr->cmsg_level == IPPROTO_IP &&
		     cmptr->cmsg_type == IP_PKTINFO ) {
			memcpy(&pktinfo, CMSG_DATA(cmptr), sizeof(struct in_pktinfo));
			memcpy(&myaddr.sin_addr, &pktinfo.ipi_addr, sizeof(struct in_addr));
		}
#endif
	}

	/* obtain the address in human readable form */
	a = inet_ntop(PF_INET, &myaddr.sin_addr, buf, sizeof(buf));
	p = ntohs(myaddr.sin_port);
	UD_DEBUG_MCAST("shit went to: host %s port %d\n", a, p);
#endif

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
#endif	/* 0 */


static void __attribute__((unused))
nothing(job_t j)
{
	return;
}

void
ud_print_mcast4(job_t j)
{
	ssize_t nwrit;

	/* the actual write */
	nwrit = sendto(j->sock, j->buf, j->blen, 0,
		       (struct sockaddr*)&j->sa,
		       j->sa.sin6_family == PF_INET6
		       ? sizeof(struct sockaddr_in6)
		       : sizeof(struct sockaddr_in));
	return;
}


static void
s2s_hy_cb(EV_P_ ev_timer *w, int revents)
{
	char buf[4 * sizeof(uint16_t)];

	UD_DEBUG("boasting about my balls ... god, are they big\n");
	/* say hy */
	udpc_hy_pkt(buf, UDPC_PKTSRC_UNK);
	/* ship to m4cast addr */
	(void)sendto(lsock, buf, countof(buf), 0,
		     (struct sockaddr*)&__sa4, sizeof(struct sockaddr_in));
	/* ship to m6cast addr */
	(void)sendto(lsock, buf, countof(buf), 0,
		     (struct sockaddr*)&__sa6, sizeof(struct sockaddr_in6));

#if 0
	/* s2s msg */
	(void)sendto(lsock, s2s_oi, s2s_oi_len, 0,
		     (struct sockaddr*)&__sa4, sizeof(struct sockaddr_in));
	(void)sendto(lsock, s2s_oi, s2s_oi_len, 0,
		     (struct sockaddr*)&__sa6, sizeof(struct sockaddr_in6));
#endif
	/* restart the timer, this will get us an id eventually */
	ev_timer_again(EV_A_ w);
	return;
}

void
ud_hyrpl_job(job_t j)
{
	return;
}


int
ud_attach_mcast4(EV_P)
{
	ev_io *srv_watcher = &__srv_watcher;
	ev_timer *s2s_watcher = &__s2s_watcher;

	/* get us a global sock */
	lsock = mcast46_listener_init();

	if (UNLIKELY(lsock < 0)) {
		return -1;
	}

	/* obtain the hostname */
	(void)gethostname(s2s_oi + 3, countof(s2s_oi));
	/* \n-ify */
	s2s_oi[s2s_oi_len = strlen(s2s_oi)] = '\n';
	s2s_oi_len++;

	/* initialise an io watcher, then start it */
	ev_io_init(srv_watcher, mcast_inco_cb, lsock, EV_READ);
	ev_io_start(EV_A_ srv_watcher);

	/* init the s2s timer, this one says `hy' until an id was negotiated */
	ev_timer_init(s2s_watcher, s2s_hy_cb, 0.0, S2S_BRAG_RATE);
	ev_timer_start(EV_A_ s2s_watcher);
	return 0;
}

int
ud_detach_mcast4(EV_P)
{
	ev_io *srv_watcher = &__srv_watcher;
	ev_timer *s2s_watcher = &__s2s_watcher;

	/* stop the guy that watches the socket */
	ev_io_stop(EV_A_ srv_watcher);

	/* stop the timer */
	ev_timer_stop(EV_A_ s2s_watcher);

	if (LIKELY(lsock >= 0)) {
		/* drop multicast group membership */
		__mcast4_leave_group(lsock, &mreq4);
		__mcast6_leave_group(lsock, &mreq6);
		/* and kick the socket */
		mcast_listener_deinit(lsock);
	}
	return 0;
}

/* mcast4.c ends here */
