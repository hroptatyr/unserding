/*** mcast.h -- unserding network intrinsics
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

#if !defined INCLUDED_mcast_h_
#define INCLUDED_mcast_h_

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define UD_NETWORK_SERVICE	8653
#define UD_NETWORK_SERVSTR	"8653"
/* 239.0.0.0/8 are organisational solicited v4 mcast addrs */
#define UD_MCAST4_ADDR		"239.86.53.1"
#define UD_MCAST4S2S_ADDR	"239.86.53.3"
/* http://www.iana.org/assignments/ipv6-multicast-addresses/ lists us 
 * as ff0x:0:0:0:0:0:0:134 */
/* link-local */
#define UD_MCAST6_LINK_LOCAL	"ff02::134"
/* site-local */
#define UD_MCAST6_SITE_LOCAL	"ff05::134"

#define UD_MCAST6_ADDR		UD_MCAST6_SITE_LOCAL

/* our grand unified sockaddr thingie */
typedef union ud_sockaddr_u ud_sockaddr_u;
typedef union ud_sockaddr_u *ud_sockaddr_t;
typedef const union ud_sockaddr_u *ud_const_sockaddr_t;

union ud_sockaddr_u {
		struct sockaddr_storage sas;
		struct sockaddr sa;
		struct sockaddr_in sa4;
		struct sockaddr_in6 sa6;
};


#if defined __INTEL_COMPILER
#pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */
/* sockaddr stuff */
static inline short unsigned int
ud_sockaddr_fam(ud_const_sockaddr_t sa)
{
	return sa->sa.sa_family;
}

static inline short unsigned int
ud_sockaddr_4port(ud_const_sockaddr_t sa)
{
	return ntohs(sa->sa4.sin_port);
}

static inline short unsigned int
ud_sockaddr_6port(ud_const_sockaddr_t sa)
{
	return ntohs(sa->sa6.sin6_port);
}

static inline short unsigned int
ud_sockaddr_port(ud_const_sockaddr_t sa)
{
	/* should be properly switched? */
	return ntohs(sa->sa6.sin6_port);
}

static inline void
ud_sockaddr_set_6port(ud_sockaddr_t sa, uint16_t port)
{
	sa->sa6.sin6_port = htons(port);
	return;
}

static inline void
ud_sockaddr_set_4port(ud_sockaddr_t sa, uint16_t port)
{
	sa->sa4.sin_port = htons(port);
	return;
}

static inline void
ud_sockaddr_set_port(ud_sockaddr_t sa, uint16_t port)
{
	/* should be properly switched? */
	sa->sa6.sin6_port = htons(port);
	return;
}

static inline const void*
ud_sockaddr_4addr(ud_const_sockaddr_t sa)
{
	return &sa->sa4.sin_addr;
}

static inline const void*
ud_sockaddr_6addr(ud_const_sockaddr_t sa)
{
	return &sa->sa6.sin6_addr;
}

static inline const void*
ud_sockaddr_addr(ud_const_sockaddr_t sa)
{
	switch (ud_sockaddr_fam(sa)) {
	case AF_INET6:
		return ud_sockaddr_6addr(sa);
	case AF_INET:
		return ud_sockaddr_4addr(sa);
	default:
		return NULL;
	}
}

static inline void
ud_sockaddr_ntop(char *restrict buf, size_t len, ud_const_sockaddr_t sa)
{
	short unsigned int fam = ud_sockaddr_fam(sa);
	const void *saa = ud_sockaddr_addr(sa);
	(void)inet_ntop(fam, saa, buf, len);
	return;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_mcast_h_ */
