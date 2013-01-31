/*** unserding.c -- unserding library definitions
 *
 * Copyright (C) 2008-2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */
#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif	/* HAVE_SYS_SOCKET_H */
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif	/* HAVE_NETINET_IN_H */
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif	/* HAVE_ARPA_INET_H */
#if defined HAVE_NET_IF_H
# include <net/if.h>
#endif	/* HAVE_NET_IF_H */
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif	/* HAVE_ERRNO_H */
#include <sys/mman.h>

/* our master include */
#include "unserding.h"
#include "ud-nifty.h"
#include "ud-sock.h"
#include "ud-sockaddr.h"
#include "ud-private.h"
#include "boobs.h"

#if !defined IPPROTO_IPV6
# error system not fit for ipv6 transport
#endif	/* IPPROTO_IPV6 */

#if defined DEBUG_FLAG
# include <stdio.h>
# define UDEBUG(args...)	fprintf(stderr, args)
# else	/* !DEBUG_FLAG */
# define UDEBUG(args...)
#endif	/* DEBUG_FLAG */

/* guaranteed by IPv6 */
#define MIN_MTU		(1280U)
/* mtu for ethernet */
#define ETH_MTU		(1492U)
/* high-3-bits MTU, 1024 + 512 + 256 */
#define H3B_MTU		(1024U + 512U + 256U)
/* control messages are shorter */
#define CTRL_MTU	(80U)

#define UDP_MULTICAST_TTL	64

/* magic string to identify unserding packets */
#define UD_PROTO_INI	0x5544/*UD*/

typedef struct __sock_s *__sock_t;

struct ud_hdr_s {
	uint16_t ini;
	uint16_t pno;
	uint16_t cmd;
	uint16_t magic;
	char pl[];
};

union ud_buf_u {
	struct {
		struct ud_hdr_s hdr;
		/* payload */
		char pl[];
	};
	char buf[ETH_MTU];
};

union ud_ctrl_u {
	struct {
		struct ud_hdr_s hdr;
		/* payload */
		char pl[];
	};
	char buf[CTRL_MTU];
};

/* our private view on ud_sock_s */
struct __sock_s {
	union {
		struct ud_sock_s pub[1];
		/* non const version of PUB */
		struct {
			int fd;
			uint32_t fl;
			const void *data;
			char priv[];
		};
	};

	/* a secondary fd for pub'ing */
	int fd_send;

	/* stuff used to configure the thing */
	struct ud_sockopt_s opt;

	/* packet within */
	int pno;
	/* service we're after */
	ud_svc_t svc;

	struct ud_sockaddr_s src[1];
	struct ud_sockaddr_s dst[1];

	/* our membership */
	struct ipv6_mreq ALGN16(memb[1]);

	/** total number of received bytes in buffer */
	size_t nrd;
	/** offset to which packet has been checked (in B) */
	size_t nck;
	union ud_buf_u ALGN16(recv);

	/** total number of sent bytes in buffer */
	size_t nwr;
	/** offset to which packet has been packed (in B) */
	size_t npk;
	union ud_buf_u ALGN16(send);
};


/* helpers */
static inline void*
mmap_mem(size_t z)
{
	void *res = mmap(NULL, z, PROT_MEM, MAP_MEM, -1, 0);

	if (UNLIKELY(res == MAP_FAILED)) {
		return NULL;
	}
	return res;
}

static inline void
munmap_mem(void *p, size_t z)
{
	munmap(p, z);
	return;
}


static int
mc6_loop(int s, int on)
{
#if defined IPV6_MULTICAST_LOOP
	/* don't loop */
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(on));
#else  /* !IPV6_MULTICAST_LOOP */
# warning multicast looping cannot be turned on or off
#endif	/* IPV6_MULTICAST_LOOP */
	return on;
}

static int
mc6_join_group(int s, struct ud_sockaddr_s *sa, struct ipv6_mreq *r)
{
	if (UNLIKELY(sa->sz == 0)) {
		return -1;
	}
	/* set up the multicast group and join it */
	r->ipv6mr_multiaddr = sa->sa.sa6.sin6_addr;
	r->ipv6mr_interface = sa->sa.sa6.sin6_scope_id;

	/* now truly join */
	return setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, r, sizeof(*r));
}

static void
mc6_leave_group(int s, struct ipv6_mreq *mreq)
{
	/* drop mcast6 group membership */
	setsockopt(s, IPPROTO_IPV6, IPV6_LEAVE_GROUP, mreq, sizeof(*mreq));
	return;
}


/* socket goodies */
static int
mc6_socket(void)
{
	volatile int s;

	/* try v6 first */
	if ((s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		return -1;
	}

#if defined IPV6_V6ONLY
	{
		int yes = 1;
		setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes));
	}
#endif	/* IPV6_V6ONLY */
	/* be less blocking */
	setsock_nonblock(s);
	return s;
}

static int
mc6_set_dest(
	struct ud_sockaddr_s *restrict s,
	const char *addr, short unsigned int port, const char *nicn)
{
	sa_family_t fam = AF_INET6;

	/* we pick link-local here for simplicity */
	if (inet_pton(fam, addr, &s->sa.sa6.sin6_addr) < 0) {
		return -1;
	}
	/* set destination address */
	s->sa.sa6.sin6_family = fam;
	/* port as well innit */
	s->sa.sa6.sin6_port = htons(port);
	/* set the flowinfo */
	s->sa.sa6.sin6_flowinfo = 0;
	/* scope id */
#if defined HAVE_NET_IF_H
	if (nicn != NULL) {
		s->sa.sa6.sin6_scope_id = if_nametoindex(nicn);
	} else {
		s->sa.sa6.sin6_scope_id = 0U;
	}
#else  /* !HAVE_NET_IF_H */
	s->sa.sa6.sin6_scope_id = 0U;
#endif	/* HAVE_NET_IF_H */
	/* also store the length */
	s->sz = sizeof(s->sa.sa6);
	return 0;
}

static int
mc6_set_pub(int s)
{
	union ud_sockaddr_u sa = {
		.sa6 = {
			.sin6_family = AF_INET6,
			.sin6_addr = IN6ADDR_ANY_INIT,
			.sin6_port = 0,
			.sin6_scope_id = 0,
		},
	};

	/* as a courtesy to tools bind the channel */
	return bind(s, &sa.sa, sizeof(sa));
}

static int
mc6_set_sub(int s, short unsigned int port)
{
	union ud_sockaddr_u sa = {
		.sa6 = {
			.sin6_family = AF_INET6,
			.sin6_addr = IN6ADDR_ANY_INIT,
			.sin6_port = htons(port),
			.sin6_scope_id = 0,
		},
	};
	int opt;

	/* allow many many many subscribers on that port */
	setsock_reuseaddr(s);

	/* turn multicast looping on */
	mc6_loop(s, 1);
#if defined IPV6_V6ONLY
	opt = 0;
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
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
#if defined IPV6_MULTICAST_HOPS
	opt = UDP_MULTICAST_TTL;
	/* turn into a mcast sock and set a TTL */
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &opt, sizeof(opt));
#endif	/* IPV6_MULTICAST_HOPS */

	/* we used to retry upon failure, but who cares */
	if (bind(s, &sa.sa, sizeof(sa)) < 0) {
		return -1;
	}
	return 0;
}

static int
mc6_unset_pub(int UNUSED(s))
{
	/* do fuckall */
	return 0;
}

static int
mc6_unset_sub(int s)
{
	/* linger the sink sock */
	setsock_linger(s, 1);
	return 0;
}


/* implementation of public interface */
ud_sock_t
ud_socket(struct ud_sockopt_s opt)
{
	__sock_t res;
	int s = -1;
	int s2 = -1;

#define MODE_SUBP(fl)	(fl & UD_SUB)
#define MODE_PUBP(fl)	(fl & UD_PUB)

	/* check if someone tries to trick us */
	if (UNLIKELY(opt.mode == UD_NONE)) {
		goto out;
	}
	/* check the rest of opt */
	if (opt.addr == NULL) {
		opt.addr = UD_MCAST6_SITE_LOCAL;
	}
	if (opt.port == 0) {
		opt.port = UD_NETWORK_SERVICE;
	}

	/* do all the socket magic first, so we don't waste memory */
	if (UNLIKELY((s = s2 = mc6_socket()) < 0)) {
		goto out;
	} else if (MODE_SUBP(opt.mode) && MODE_PUBP(opt.mode)) {
		/* we've got a PUBSUB request */
		if (UNLIKELY((s2 = mc6_socket()) < 0)) {
			/* oh brilliant, what now?
			 * technically we could live on just one socket */
			goto clos_out;
		}
	}

	if (0) {
		/* for the aesthetical value */
		;
	} else if (MODE_SUBP(opt.mode) && mc6_set_sub(s, opt.port) < 0) {
		goto clos2_out;
	} else if (MODE_PUBP(opt.mode) && mc6_set_pub(s2) < 0) {
		goto clos2_out;
	}

	/* fingers crossed we don't waste memory (on hugepage systems) */
	if (UNLIKELY((res = mmap_mem(sizeof(*res))) == NULL)) {
		goto clos2_out;
	}

	/* fill in res */
	res->fd = s;
	res->fd_send = s2;
	res->fl = 0U;
	res->data = NULL;

	/* keep a copy of how we configured this sockets */
	res->opt = opt;

	/* destination address now, use SILO by default for now */
	mc6_set_dest(res->dst, opt.addr, opt.port, opt.intf);
	/* set the length of the storage for the source address */
	res->src->sz = sizeof(res->src->sa);

	/* join the mcast group(s) */
	if (MODE_SUBP(opt.mode) && mc6_join_group(s, res->dst, res->memb) < 0) {
		goto munm_out;
	}
	/* service for tools like ud-dealer */
	(void)connect(res->fd_send, &res->dst->sa.sa, res->dst->sz);
	return (ud_sock_t)res;

munm_out:
	munmap_mem(res, sizeof(*res));
clos2_out:
	if (s != s2 && s2 >= 0) {
		close(s2);
	}
clos_out:
	close(s);
out:
	return NULL;
}

int
ud_close(ud_sock_t s)
{
	__sock_t us = (__sock_t)s;
	int fd = us->fd;

	switch (us->opt.mode) {
	case UD_PUBSUB:
		mc6_unset_pub(us->fd_send);
		close(us->fd_send);
		/*@fallthrough@*/
	case UD_SUB:
		mc6_unset_sub(fd);
		/* leave the mcast group */
		mc6_leave_group(fd, us->memb);
		break;
	case UD_PUB:
		mc6_unset_pub(fd);
		break;
	default:
		break;
	}

	munmap_mem(us, sizeof(*us));
	return close(fd);
}

/* actual I/O */
int
ud_flush(ud_sock_t sock)
{
	__sock_t us = (__sock_t)sock;

	if (LIKELY(us->npk > 0U)) {
		ssize_t nwr;
		const void *b = us->send.buf;
		size_t z = us->npk + sizeof(us->send.hdr);

		us->send.hdr.ini = htobe16(UD_PROTO_INI);
		us->send.hdr.pno = htobe16(us->pno);
		us->send.hdr.cmd = htobe16(us->svc);
		us->send.hdr.magic = htobe16(0xda7a);

		if ((nwr = send(us->fd_send, b, z, 0)) < 0) {
			return -1;
		} else if ((size_t)nwr < z) {
			/* should we try a resend? */
			;
		}

		/* update indexes */
		us->npk = 0U;
		us->nwr = 0U;

		/* update our counters and stuff */
		us->pno++;
	}
	/* definitely reset svc field */
	us->svc = 0U;
	return 0;
}

int
ud_dscrd(ud_sock_t sock)
{
	__sock_t us = (__sock_t)sock;

	if (UNLIKELY(us->nrd == 0U)) {
		/* oh, we shall read the shebang off the wire innit? */
		void *restrict b = us->recv.buf;
		size_t z = sizeof(us->recv.buf);
		struct sockaddr *restrict sa = &us->src->sa.sa;
		socklen_t *restrict sz = &us->src->sz;
		ssize_t nrd;

		if ((nrd = recvfrom(us->fd, b, z, 0, sa, sz)) < 0) {
			return -1;
		} else if ((nrd -= sizeof(us->recv.hdr)) < 0) {
			return -1;
		} else if (be16toh(us->recv.hdr.ini) != UD_PROTO_INI) {
			/* for transition purposes we don't fuck off
			 * right here but instead we check if the magic
			 * matches to ease migrating from older setups */
			uint16_t magic = be16toh(us->recv.hdr.magic);

			switch (magic) {
			case 0xbeef/*UDPC_PKTFLO_NOW_ONE*/:
			case 0xbeee/*UDPC_PKTFLO_NOW_MANY*/:
			case 0xcaff/*UDPC_PKTFLO_SOON_ONE*/:
			case 0xcafe/*UDPC_PKTFLO_SOON_MANY*/:
			case 0xda7a/*UDPC_PKTFLO_DATA*/:
				break;
			default:
				UDEBUG("magic fucked %04hx\n", magic);
				return -1;
			}
		}

		/* update indexes */
		us->nrd = nrd;
	} else {
		/* update indexes */
		us->nrd = 0U;
	}
	/* pretend we haven't checked anything */
	us->nck = 0U;
	return 0;
}

static inline bool
__msg_fits_p(__sock_t s, size_t len)
{
	return s->npk + 2U + len <= sizeof(s->send.buf) - sizeof(s->send.hdr);
}

static inline bool
__svc_same_p(__sock_t s, ud_svc_t svc)
{
	return s->svc == 0U || svc == s->svc;
}

int
ud_pack_msg(ud_sock_t sock, struct ud_msg_s msg)
{
	__sock_t us = (__sock_t)sock;
	uint8_t z = (uint8_t)msg.dlen;
	const char *d = msg.data;
	char *p;

	if (UNLIKELY(!__msg_fits_p(us, z) || !__svc_same_p(us, msg.svc))) {
		/* send what we've got */
		if (UNLIKELY(ud_flush(sock)) < 0) {
			/* nah, don't pack up new stuff,
			 * we need to get rid of the old shit first
			 * actually this should be configurable behaviour */
			return -1;
		}
	}

	/* update service slot, always */
	us->svc = msg.svc;

	/* now copy the blob */
#define UDPC_TYPE_DATA	(0x0c)
	p = us->send.pl + us->npk;
	*p++ = UDPC_TYPE_DATA;
	*p++ = z;
	memcpy(p, d, z);
	p += z;

	/* and update counters */
	us->npk = p - us->send.pl;
	return 0;
}

int
ud_pack(ud_sock_t sock, ud_svc_t svc, const void *data, size_t dlen)
{
	return ud_pack_msg(sock, (struct ud_msg_s){
				.svc = svc, .data = data, .dlen = dlen});
}

static inline __attribute__((pure)) bool
__ctrl_msg_p(ud_svc_t svc)
{
	return UD_CHN(svc) == UD_CHN_CTRL;
}

int
ud_chck_msg(struct ud_msg_s *restrict tgt, ud_sock_t sock)
{
	__sock_t us = (__sock_t)sock;
	ud_svc_t svc;
	char *p;

	if (UNLIKELY(us->nck >= us->nrd)) {
		/* we need another dose */
		if (UNLIKELY(ud_dscrd(sock)) < 0) {
			/* nah, don't pack up new stuff,
			 * we need to get rid of the old shit first
			 * actually this should be configurable behaviour */
			return -1;
		} else if (us->nck >= us->nrd) {
			/* no more data, just fuck off */
			return -1;
		}
	}

	/* check for control messages */
	if (UNLIKELY(__ctrl_msg_p(svc = be16toh(us->recv.hdr.cmd)))) {
		(void)ud_chck_cmsg(tgt, sock);
	}

	/* now copy the blob */
	p = us->recv.pl + us->nck;
	if ((*p++ != UDPC_TYPE_DATA)) {
		us->nrd = us->nck = 0U;
		return -1;
	}
	tgt->dlen = *p++;
	tgt->data = p;

	/* and the message service */
	tgt->svc = svc;

	/* and update counters */
	us->nck += tgt->dlen + 1U/*for UDPC_TYPE_DATA*/ + 1U/*for length*/;
	return 0;
}

ssize_t
ud_chck(ud_svc_t *svc, void *restrict tgt, size_t tsz, ud_sock_t sock)
{
	struct ud_msg_s msg = {0};

	if (ud_chck_msg(&msg, sock) < 0) {
		return -1;
	}
	/* otherwise copy to user buffer */
	if (UNLIKELY(msg.dlen > tsz)) {
		msg.dlen = tsz;
	}
	*svc = msg.svc;
	memcpy(tgt, msg.data, msg.dlen);
	return msg.dlen;
}

const struct sockaddr*
ud_socket_addr(ud_sock_t s)
{
	__sock_t us = (__sock_t)s;
	return (const struct sockaddr*)&us->dst->sa;
}

int
ud_get_aux(struct ud_auxmsg_s *restrict tgt, ud_sock_t sock)
{
	__sock_t us = (__sock_t)sock;

	if (UNLIKELY(us->nrd == 0UL)) {
		return -1;
	}
	/* zero-copy */
	tgt->src = (const struct sockaddr*)&us->src->sa;
	tgt->pno = be16toh(us->recv.hdr.pno);
	tgt->svc = be16toh(us->recv.hdr.cmd);
	tgt->len = us->nrd;
	return 0;
}


/* now come private bits of the API, touch'n'go:
 * these may or may not disappear, change, reappear, or even go in the
 * public API one day */
#include "svc-pong.h"

/* control packs */
int
ud_pack_cmsg(ud_sock_t sock, struct ud_msg_s msg)
{
	static union ud_ctrl_u ALGN16(ctrl);
	__sock_t us = (__sock_t)sock;
	char *p;

	if (UNLIKELY(msg.dlen + 2U > sizeof(ctrl) - sizeof(ctrl.hdr))) {
		/* this would be a protocol deficiency */
		return -1;
	}

	/* now copy the blob */
#define UDPC_TYPE_DATA	(0x0c)
	p = ctrl.pl;
	*p++ = UDPC_TYPE_DATA;
	*p++ = (uint8_t)msg.dlen;
	memcpy(p, msg.data, msg.dlen);
	p += msg.dlen;

	/* and flush */
	{
		ssize_t nwr;
		const void *b = ctrl.buf;
		size_t z = p - ctrl.pl + sizeof(ctrl.hdr);

		ctrl.hdr.ini = htobe16(UD_PROTO_INI);
		ctrl.hdr.pno = htobe16(us->pno);
		ctrl.hdr.cmd = htobe16(msg.svc);
		ctrl.hdr.magic = htobe16(0xda7a);

		if ((nwr = send(us->fd_send, b, z, 0)) < 0) {
			return -1;
		} else if ((size_t)nwr < z) {
			/* should we try a resend? */
			;
		}

		/* update our counters and stuff */
		us->pno++;
	}
	return 0;
}

int
ud_chck_cmsg(struct ud_msg_s *restrict UNUSED(tgt), ud_sock_t sock)
{
	__sock_t us = (__sock_t)sock;
	ud_svc_t svc;

	/* check for control messages */
	if (UNLIKELY(!__ctrl_msg_p(svc = be16toh(us->recv.hdr.cmd)))) {
		/* don't discard or update */
		return -1;
	}

	/* check what they want */
	switch (svc & 0x00ff) {
	case UD_SVC_CMD:
	case UD_SVC_TIME:
	default:
		break;
	case UD_SVC_PING:
		ud_pack_pong(sock, 1);
		break;
	}
	return 0;
}

/* unserding.c ends here */
