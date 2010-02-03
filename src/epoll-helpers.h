/*** epoll-helpers.h -- socket watching through epoll
 *
 * Copyright (C) 2009, 2010 Sebastian Freundt
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

#if !defined INCLUDED_epoll_helpers_h_
#define INCLUDED_epoll_helpers_h_

#if !defined STATIC_HELPERS
# define INLINE		inline
#else  /* STATIC_HELPERS */
# define INLINE		__attribute__((unused))
#endif	/* !STATIC_HELPERS */

#include <string.h>
#include <sys/epoll.h>
#include "ud-sock.h"

/**
 * Context type for reentrant passing around. */
typedef struct ep_ctx_s *ep_ctx_t;

struct ep_ctx_s {
	/* epoll socket */
	int sock;
	/* number of events in the following flex array */
	int nev;
	/* flex array of events, 8b-aligned */
	struct epoll_event ev[];
};

#define FLEX_EP_CTX(n)						\
	union {							\
		char dummy[sizeof(struct ep_ctx_s) +		\
			   n * sizeof(struct epoll_event)];	\
		struct ep_ctx_s ep_ctx;				\
	}
#define FLEX_EP_CTX_INITIALISER(n)			\
	{.sock = -1, .nev = n, .ev = {[n - 1] = {}}}

/**
 * Singleton, initialise as often as you like, however the NEV slot is
 * always set and the EV section is always rinsed. */
static INLINE void
init_ep_ctx(ep_ctx_t epg, int nev)
{
	epg->nev = nev;

	/* rinse */
	memset(epg->ev, 0, nev * sizeof(*epg->ev));

	/* singleton mode, if sock exists, dont bother */
	if (epg->sock >= 0) {
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
	return;
}

/**
 * Singleton, free as often as you like. */
static INLINE void
free_ep_ctx(ep_ctx_t epg)
{
	if (LIKELY(epg->sock >= 0)) {
		/* close the epoll socket */
		close(epg->sock);
		/* wipe */
		epg->sock = -1;
	}
	return;
}

static INLINE int
ep_prep(ep_ctx_t epg, int s, int flags)
{
	struct epoll_event ev = {.events = flags};
	/* add S to the epoll descriptor EPFD */
	return epoll_ctl(epg->sock, EPOLL_CTL_ADD, s, &ev);
}

static INLINE int
ep_wait(ep_ctx_t epg, int timeout)
{
	/* wait and return */
	return epoll_wait(epg->sock, epg->ev, epg->nev, timeout);
}

static INLINE int
ep_fini(ep_ctx_t epg, int s)
{
	/* remove S from the epoll descriptor EPFD */
	return epoll_ctl(epg->sock, EPOLL_CTL_DEL, s, NULL);
}

/* convenience */
#define STD_FLAGS	EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLRDHUP
static inline int
ep_prep_reader(ep_ctx_t epg, int s)
{
	return ep_prep(epg, s, STD_FLAGS | EPOLLIN);
}

static inline int
ep_prep_writer(ep_ctx_t epg, int s)
{
	return ep_prep(epg, s, STD_FLAGS | EPOLLOUT);
}

static inline int
ep_prep_et_reader(ep_ctx_t epg, int s)
{
	return ep_prep(epg, s, STD_FLAGS | EPOLLET | EPOLLIN);
}

static inline int
ep_prep_et_writer(ep_ctx_t epg, int s)
{
	return ep_prep(epg, s, STD_FLAGS | EPOLLET | EPOLLOUT);
}

#endif	/* INCLUDED_epoll_helpers_h_ */
