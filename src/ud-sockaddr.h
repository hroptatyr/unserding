/*** ud-sockaddr.h -- sockaddr unification
 *
 * Copyright (C) 2011-2013  Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of the unserding.
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
#if !defined INCLUDED_ud_sockaddr_h_
#define INCLUDED_ud_sockaddr_h_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif /* __cplusplus */

/* sockaddr union */
typedef union ud_sockaddr_u *ud_sockaddr_t;
typedef const union ud_sockaddr_u *ud_const_sockaddr_t;

union ud_sockaddr_u {
	struct sockaddr_storage sas;
	struct sockaddr sa;
	struct sockaddr_in6 sa6;
};

struct ud_sockaddr_s {
	socklen_t sz;
	union ud_sockaddr_u sa;
};


static inline short unsigned int
ud_sockaddr_fam(ud_const_sockaddr_t sa)
{
	return sa->sa.sa_family;
}

static inline short unsigned int
ud_sockaddr_port(ud_const_sockaddr_t sa)
{
	return ntohs(sa->sa6.sin6_port);
}

static inline const void*
ud_sockaddr_addr(ud_const_sockaddr_t sa)
{
	return &sa->sa6.sin6_addr;
}

static inline void
ud_sockaddr_ntop(char *restrict buf, size_t len, ud_const_sockaddr_t sa)
{
	short unsigned int fam = ud_sockaddr_fam(sa);
	const void *saa = ud_sockaddr_addr(sa);
	(void)inet_ntop(fam, saa, buf, len);
	return;
}

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_ud_sockaddr_h_ */
