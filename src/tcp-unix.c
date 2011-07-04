/*** tcp-unix.c -- tcp/unix handlers
 *
 * Copyright (C) 2011 Sebastian Freundt
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
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
/* we just need the headers hereof and hope that unserding used the same ones */
#include <ev.h>
#include "unserding-private.h"
#include "unserding-nifty.h"
#include "unserding-dbg.h"
#include "ud-sock.h"
#include "tcp-unix.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:2259)
# pragma warning (disable:177)
#endif	/* __INTEL_COMPILER */

#if defined DEBUG_FLAG
# define UD_DEBUG_TU(args...)				\
	fprintf(logout, "[unserding/input/tcpunix] " args)
#else
# define UD_DEBUG_MCAST(args...)
#endif

typedef enum {
	UD_CONN_UNK,
	UD_CONN_LSTN,
	UD_CONN_RD,
	UD_CONN_WR,
	UD_CONN_INOT,
} ud_ctype_t;

struct __wbuf_s {
	union {
		char *buf;
		const char *cbuf;
	};
	size_t len;
	size_t nwr;

	/* inco neighbour */
	ud_conn_t neigh;
};

/* helpers for careless tcp/unix writing */
struct __conn_s {
	union {
		ev_io io[1];
		ev_stat st[1];
	};

	ud_ctype_t ty;

	/* for write buffers */
	struct __wbuf_s wb[1];

	ud_cbcb_f iord;
	union {
		ud_ccb_f clos;
		ud_cicb_f inot;
	};

	ud_conn_t parent;
	void *data;
};

/* helpers for the buffer writers */
static size_t nconns = 0;
static struct __conn_s *conns = NULL;

static ud_conn_t
make_conn(ud_ctype_t ty)
{
	ud_conn_t res;

	for (size_t i = 0; i < nconns; i++) {
		if (conns[i].ty == UD_CONN_UNK) {
			res = conns + i;
			goto out;
		}
	}
	/* create one, check for resize first */
	if ((nconns % 16) == 0) {
		conns = realloc(conns, 16 * sizeof(*conns));
		memset(conns + nconns, 0, 16 * sizeof(*conns));
	}
	res = conns + nconns++;
out:
	res->ty = ty;
	return res;
}

static void
free_conn(ud_conn_t c)
{
	memset(c, 0, sizeof(*c));
	return;
}


void*
ud_conn_get_data(ud_conn_t c)
{
	return c->data;
}

void
ud_conn_put_data(ud_conn_t c, void *data)
{
	c->data = data;
	return;
}


/* connection mumbo-jumbo */
static void *gloop = NULL;

static void
__shut_sock(int s)
{
	shutdown(s, SHUT_RDWR);
	close(s);
	return;
}

static void
clos_conn(EV_P_ ud_conn_t c, bool force)
{
	if (c->clos && c->clos(c, c->data) < 0 && !force) {
		return;
	}
	fsync(c->io->fd);
	ev_io_stop(EV_A_ c->io);
	__shut_sock(c->io->fd);
	free_conn(c);
	return;
}

static void
clos_inot(EV_P_ ud_conn_t c, bool UNUSED(force))
{
	ev_stat_stop(EV_A_ c->st);
	free_conn(c);
	return;
}

/* we could take args like listen address and port number */
static int
conn_listener_net(uint16_t port)
{
#if defined IPPROTO_IPV6
	static struct sockaddr_in6 __sa6 = {
		.sin6_family = AF_INET6,
		.sin6_addr = IN6ADDR_ANY_INIT
	};
	volatile int s;

	/* non-constant slots of __sa6 */
	__sa6.sin6_port = htons(port);

	if (LIKELY((s = socket(PF_INET6, SOCK_STREAM, 0)) >= 0)) {
		/* likely case upfront */
		;
	} else {
		UD_DEBUG_TU("socket() failed ... I'm clueless now\n");
		return s;
	}

#if defined IPV6_V6ONLY
	setsockopt_int(s, IPPROTO_IPV6, IPV6_V6ONLY, 0);
#endif	/* IPV6_V6ONLY */
#if defined IPV6_USE_MIN_MTU
	/* use minimal mtu */
	setsockopt_int(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU, 1);
#endif
#if defined IPV6_DONTFRAG
	/* rather drop a packet than to fragment it */
	setsockopt_int(s, IPPROTO_IPV6, IPV6_DONTFRAG, 1);
#endif
#if defined IPV6_RECVPATHMTU
	/* obtain path mtu to send maximum non-fragmented packet */
	setsockopt_int(s, IPPROTO_IPV6, IPV6_RECVPATHMTU, 1);
#endif
	setsock_reuseaddr(s);
	setsock_reuseport(s);

	/* we used to retry upon failure, but who cares */
	if (bind(s, (struct sockaddr*)&__sa6, sizeof(__sa6)) < 0 ||
	    listen(s, 2) < 0) {
		UD_DEBUG_TU("bind() failed, errno %d\n", errno);
		close(s);
		return -1;
	}
	return s;

#else  /* !IPPROTO_IPV6 */
	return -1;
#endif	/* IPPROTO_IPV6 */
}

static int
conn_listener_uds(const char *sock_path)
{
	static struct sockaddr_un __s = {
		.sun_family = AF_LOCAL,
	};
	volatile int s;
	size_t sz;

	if (LIKELY((s = socket(PF_LOCAL, SOCK_STREAM, 0)) >= 0)) {
		/* likely case upfront */
		;
	} else {
		UD_DEBUG_TU("socket() failed ... I'm clueless now\n");
		return s;
	}

	/* bind a name now */
	strncpy(__s.sun_path, sock_path, sizeof(__s.sun_path));
	__s.sun_path[sizeof(__s.sun_path) - 1] = '\0';

	/* The size of the address is
	   the offset of the start of the filename,
	   plus its length,
	   plus one for the terminating null byte.
	   Alternatively you can just do:
	   size = SUN_LEN (&name); */
	sz = offsetof(struct sockaddr_un, sun_path) + strlen(__s.sun_path) + 1;

	/* just unlink the socket */
	unlink(sock_path);
	/* we used to retry upon failure, but who cares */
	if (bind(s, (struct sockaddr*)&__s, sz) < 0) {
		UD_DEBUG_TU("bind() failed: %s\n", strerror(errno));
		close(s);
		unlink(sock_path);
		return -1;
	}
	if (listen(s, 2) < 0) {
		UD_DEBUG_TU("listen() failed: %s\n", strerror(errno));
		close(s);
		unlink(sock_path);
		return -1;
	}
	/* allow the whole world to connect to us */
	chmod(sock_path, 0777);
	return s;
}


static void
writ_cb(EV_P_ ev_io *e, int UNUSED(re))
{
	int fd = e->fd;
	ud_conn_t c = (void*)e;
	size_t len = c->wb->len - c->wb->nwr;
	ssize_t nwr;

	if ((nwr = write(fd, c->wb->cbuf + c->wb->nwr, len)) < 0) {
		goto clo;
	}
	UD_DEBUG_TU("wrote %zd to %d\n", nwr, fd);
	if ((c->wb->nwr += nwr) < c->wb->len) {
		return;
	}
clo:
	UD_DEBUG_TU("%d not needed for write, cleaning up\n", fd);
	/* unsubscribe interest */
	ev_io_stop(EV_A_ e);

	if (c->clos) {
		/* call the user's idea of what has to be done now */
		c->clos(c, c->data);
	}
	/* remove ourselves from our neighbour's slot */
	ud_conn_put_data(c->wb->neigh, NULL);
	free_conn(c);
	return;
}

static void
data_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	char buf[4096];
	ssize_t nrd;
	ud_conn_t c = (void*)w;

	if ((nrd = read(w->fd, buf, sizeof(buf))) <= 0) {
		goto clo;
	}
	UD_DEBUG_TU("new data in sock %d\n", w->fd);
	if (c->iord && c->iord(c, buf, nrd, c->data) < 0) {
		goto clo;
	}
	return;
clo:
	for (size_t i = 0; i < nconns; i++) {
		if (conns[i].wb->neigh == c) {
			ud_conn_t n = conns + i;
			UD_DEBUG_TU("unfinished business on %p\n", c);
			return;
		}
	}
	UD_DEBUG_TU("%zd data, closing socket %d\n", nrd, w->fd);
	clos_conn(EV_A_ c, false);
	return;
}

static void
inco_cb(EV_P_ ev_io *w, int UNUSED(re))
{
/* we're tcp so we've got to accept() the bugger, don't forget :) */
	volatile int ns;
	ud_conn_t aw, par;
	struct sockaddr_storage sa;
	socklen_t sa_size = sizeof(sa);

	UD_DEBUG_TU("they got back to us %p...", w);
	if ((ns = accept(w->fd, (struct sockaddr*)&sa, &sa_size)) < 0) {
		UD_DBGCONT("accept() failed %s\n", strerror(errno));
		return;
	}

        /* make an io watcher and watch the accepted socket */
	aw = make_conn(UD_CONN_RD);
        ev_io_init(aw->io, data_cb, ns, EV_READ);
	aw->data = NULL;
	par = (void*)w;
	aw->parent = par;
	aw->iord = par->iord;
	aw->clos = par->clos;
        ev_io_start(EV_A_ aw->io);
	UD_DBGCONT("success, new sock %d %p\n", ns, aw);
	return;
}

static void
inot_cb(EV_P_ ev_stat *w, int UNUSED(re))
{
	ud_conn_t c = (void*)w;

	UD_DEBUG_TU("INOT something changed in %s...\n", w->path);
	if (c->inot && c->inot(c, w->path, &w->attr, c->data) < 0) {
		goto clo;
	}
	return;
clo:
	ev_stat_stop(EV_A_ w);
	return;
}


static ud_conn_t
init_conn_watchers(EV_P_ int s)
{
	ud_conn_t c;

	if (s < 0) {
		return NULL;
	}

        /* initialise an io watcher, then start it */
	c = make_conn(UD_CONN_LSTN);
        ev_io_init(c->io, inco_cb, s, EV_READ);
        ev_io_start(EV_A_ c->io);
	return c;
}

static ud_conn_t
init_stat_watchers(EV_P_ const char *file)
{
	ud_conn_t c;

	if (file == NULL) {
		return NULL;
	}

        /* initialise an io watcher, then start it */
	c = make_conn(UD_CONN_INOT);
        ev_stat_init(c->st, inot_cb, file, 0.);
        ev_stat_start(EV_A_ c->st);
	return c;
}


/* public funs */
ud_conn_t
make_tcp_conn(uint16_t port, ud_cbcb_f data_in, ud_ccb_f clo, void *data)
{
	volatile int sock = -1;
	ud_conn_t res = NULL;

	if (port > 0 &&
	    (sock = conn_listener_net(port)) > 0 &&
	    gloop != NULL &&
	    (res = init_conn_watchers(gloop, sock)) != NULL) {
		res->iord = data_in;
		res->clos = clo;
		res->data = data;
	}
	return res;
}

ud_conn_t
make_unix_conn(const char *path, ud_cbcb_f data_in, ud_ccb_f clo, void *data)
{
	volatile int sock = -1;
	ud_conn_t res = NULL;

	if (path != NULL &&
	    (sock = conn_listener_uds(path)) > 0 &&
	    gloop != NULL &&
	    (res =init_conn_watchers(gloop, sock)) != NULL) {
		res->iord = data_in;
		res->clos = clo;
		res->data = data;
	}
	return res;
}

ud_conn_t
make_inot_conn(const char *file, ud_cicb_f noti_cb, void *data)
{
	volatile int sock = -1;
	ud_conn_t res = NULL;

	if (file != NULL &&
	    gloop != NULL &&
	    (res = init_stat_watchers(gloop, file)) != NULL) {
		res->inot = noti_cb;
		res->data = data;
	}
	return res;
}

void*
ud_conn_fini(ud_conn_t c)
{
	void *res = c->data;

	for (size_t i = 0; i < nconns; i++) {
		/* find kids, whose parent is C */
		if (conns[i].parent == c) {
			switch (conns[i].ty) {
			case UD_CONN_LSTN:
			case UD_CONN_WR:
				clos_conn(gloop, conns + i, true);
				break;
			case UD_CONN_INOT:
				clos_inot(gloop, conns + i, true);
				break;
			default:
			case UD_CONN_UNK:
				break;
			}
		}
	}
	clos_conn(gloop, c, true);
	return res;
}

/* helpers for careless writing */
ud_conn_t
ud_write_soon(ud_conn_t conn, const char *buf, size_t len, ud_ccb_f noti_clos)
{
	ud_conn_t c;
	ssize_t nwr;
	int fd = conn->io->fd;

	/* check if request is trivial and try a test write */
	if (buf == NULL || len == 0) {
		return NULL;
	} else if ((nwr = write(fd, buf, len)) == len) {
		return NULL;
	}
	/* otherwise the user isn't so much a prick as we thought*/
	c = make_conn(UD_CONN_WR);
	/* fill in */
	c->wb->cbuf = buf;
	c->wb->len = len;
	c->wb->nwr = nwr > 0 ? nwr : 0UL;
	c->clos = noti_clos;
	c->wb->neigh = conn;
	c->parent = conn->parent;

	/* finally we pretend interest in this socket */
        ev_io_init(c->io, writ_cb, fd, EV_WRITE);
        ev_io_start(gloop, c->io);
	return c;
}

int
ud_attach_tcp_unix(EV_P_ bool UNUSED(prefer_ipv6))
{
	gloop = loop;
	return 0;
}

int
ud_detach_tcp_unix(EV_P)
{
	for (size_t i = 0; i < nconns; i++) {
		switch (conns[i].ty) {
		case UD_CONN_LSTN:
		case UD_CONN_RD:
		case UD_CONN_WR:
			/* ideally we'd close the user's data socket too
			 * alas we can't */
			clos_conn(EV_A_ conns + i, true);
			break;
		case UD_CONN_INOT:
			clos_inot(EV_A_ conns + i, true);
			break;
		case UD_CONN_UNK:
		default:
			break;
		}
	}
	gloop = NULL;
	return 0;
}

/* tcp-unix.c ends here */
