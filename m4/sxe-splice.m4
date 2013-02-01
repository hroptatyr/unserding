dnl sxe-splice.m4 -- check splice implementations
dnl
dnl Copyright (C) 2012-2013 Sebastian Freundt
dnl
dnl Author: Sebastian Freundt <freundt@ga-group.nl>
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl
dnl 2. Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl
dnl 3. Neither the name of the author nor the names of any contributors
dnl    may be used to endorse or promote products derived from this
dnl    software without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
dnl IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
dnl WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
dnl DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
dnl FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
dnl CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
dnl SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
dnl BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
dnl WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
dnl OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
dnl IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
dnl
dnl This file is part of unserding

AC_DEFUN([SXE_CHECK_TCP_SPLICE], [
dnl Usage: SXE_CHECK_TCP_SPLICE
dnl   def: sxe_cv_feat_tcp_splice yes|no
dnl   def: HAVE_TCP_SPLICE
	AC_LANG_PUSH([C])

	AC_CACHE_CHECK([if we can splice tcp sockets], [sxe_cv_feat_tcp_splice], [
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

int
main(void)
{
	int pfd[2];
	int srv;
	int cli;
	int acc;
	struct sockaddr_in6 dummy = {
		.sin6_family = AF_INET6,
		.sin6_addr = IN6ADDR_ANY_INIT,
		.sin6_port = 0,
		.sin6_scope_id = 0,
	};
	socklen_t dummz = sizeof(dummy);

	if (pipe(pfd) < 0) {
		return 2;
	} else if ((srv = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
		return 2;
	} else if ((cli = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
		return 2;
	} else if (bind(srv, (const void*)&dummy, dummz) < 0) {
		return 2;
	} else if (listen(srv, 1) < 0) {
		return 2;
	} else if (getsockname(srv, (void*)&dummy, &dummz) < 0) {
		return 2;
	} else if (connect(cli, (const void*)&dummy, dummz) < 0) {
		return 2;
	} else if ((acc = accept(srv, (void*)&dummy, &dummz)) < 0) {
		return 2;
	} else if (send(cli, "test", 4, 0) < 4) {
		return 2;
	} else if (splice(acc, NULL, pfd[1], NULL, 1280, SPLICE_F_MOVE) < 4) {
		return 1;
	} else if (splice(pfd[0], NULL, acc, NULL, 4, SPLICE_F_MOVE) < 4) {
		return 1;
	} else if (({char buf[1280]; recv(cli, buf, sizeof(buf), 0) < 4;})) {
		return 2;
	} else if (close(acc) < 0) {
		return 2;
	} else if (close(cli) < 0) {
		return 2;
	} else if (close(srv) < 0) {
		return 2;
	} else if (close(pfd[0]) < 0 || close(pfd[1]) < 0) {
		return 2;
	}
	return 0;
}
]])], [
			sxe_cv_feat_tcp_splice="yes"
		], [
			if test "${ac_status}" = "1"; then
				sxe_cv_feat_tcp_splice="no"
			else
				sxe_cv_feat_tcp_splice="vague"
			fi
		])dnl AC_RUN_IFELSE
	])dnl AC_CACHE_CHECK

	if test "${sxe_cv_feat_tcp_splice}" = "yes"; then
		AC_DEFINE([HAVE_TCP_SPLICE], [1], [dnl
whether splicing to and, more importantly, from tcp sockets is possible])
	elif test "${sxe_cv_feat_tcp_splice}" = "vague"; then
		AC_MSG_WARN([The surrounding tcp splicing test code failed.
That means tcp splicing could be possible but it cannot be checked with
the current test code.])
	fi

	AC_LANG_POP([C])
])dnl SXE_CHECK_TCP_SPLICE

AC_DEFUN([SXE_CHECK_UDP_SPLICE], [
dnl Usage: SXE_CHECK_UDO_SPLICE
dnl   def: sxe_cv_feat_udp_splice yes|no
dnl   def: HAVE_UDP_SPLICE
	AC_LANG_PUSH([C])

	AC_CACHE_CHECK([if we can splice udp sockets], [sxe_cv_feat_udp_splice], [
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

int
main(void)
{
	int pfd[2];
	int srv;
	int cli;
	struct sockaddr_in6 dummy = {
		.sin6_family = AF_INET6,
		.sin6_addr = IN6ADDR_ANY_INIT,
		.sin6_port = 0,
		.sin6_scope_id = 0,
	};
	socklen_t dummz = sizeof(dummy);

	if (pipe(pfd) < 0) {
		return 2;
	} else if ((srv = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		return 2;
	} else if ((cli = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		return 2;
	} else if (bind(srv, (const void*)&dummy, dummz) < 0) {
		return 2;
	} else if (getsockname(srv, (void*)&dummy, &dummz) < 0) {
		return 2;
	} else if (connect(cli, (const void*)&dummy, dummz) < 0) {
		return 2;
	} else if (send(cli, "test", 4, 0) < 4) {
		return 2;
	} else if (splice(srv, NULL, pfd[1], NULL, 1280, SPLICE_F_MOVE) < 4) {
		return 1;
	} else if (splice(pfd[0], NULL, srv, NULL, 4, SPLICE_F_MOVE) < 4) {
		return 1;
	} else if (({char buf[1280]; recv(cli, buf, sizeof(buf), 0) < 4;})) {
		return 2;
	} else if (close(cli) < 0) {
		return 2;
	} else if (close(srv) < 0) {
		return 2;
	} else if (close(pfd[0]) < 0 || close(pfd[1]) < 0) {
		return 2;
	}
	return 0;
}
]])], [
			sxe_cv_feat_udp_splice="yes"
		], [
			if test "${ac_status}" = "1"; then
				sxe_cv_feat_udp_splice="no"
			else
				sxe_cv_feat_udp_splice="vague"
			fi
		])dnl AC_RUN_IFELSE
	])dnl AC_CACHE_CHECK

	if test "${sxe_cv_feat_udp_splice}" = "yes"; then
		AC_DEFINE([HAVE_UDP_SPLICE], [1], [dnl
whether splicing to and, more importantly, from udp sockets is possible])
	elif test "{sxe_cv_feat_udp_splice}" = "vague"; then
		AC_MSG_WARN([The surrounding udp splicing test code failed.
That means udp splicing could be possible but it cannot be checked with
the current test code.])
	fi

	AC_LANG_POP([C])
])dnl SXE_CHECK_UDP_SPLICE
