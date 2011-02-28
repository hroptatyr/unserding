dnl sxe-events.m4 -- Event queue and things like that
dnl
dnl Copyright (C) 2005, 2006, 2007, 2008 Sebastian Freundt
dnl
dnl Author: Sebastian Freundt <hroptatyr@sxemacs.org>
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
dnl This file is part of SXEmacs.

dnl event drivers
AC_DEFUN([SXE_CHECK_LIBEVENT], [
	SXE_CHECK_HEADERS([event.h])
	SXE_CHECK_LIB_FUNCS([event], [dnl
		event_init event_loop event_add event_del event_once event_dispatch])

	if test "$ac_cv_header_event_h" = "yes" -a \
		"$ac_cv_lib_event___event_init" = "yes" -a \
		"$ac_cv_lib_event___event_loop" = "yes" -a \
		"$ac_cv_lib_event___event_add" = "yes" -a \
		"$ac_cv_lib_event___event_del" = "yes" -a \
		"$ac_cv_lib_event___event_once" = "yes" -a \
		"$ac_cv_lib_event___event_dispatch" = "yes"; then
		AC_DEFINE([HAVE_LIBEVENT], [1],
			[Whether all necessary functions could be found in libevent.])
		sxe_cv_feat_libevent="yes"
		have_libevent="yes"
	else
		sxe_cv_feat_libevent="no"
		have_libevent="no"
	fi
])dnl SXE_CHECK_LIBEVENT

AC_DEFUN([SXE_CHECK_LIBEV], [
	## defines sxe_cv_feat_libev
	SXE_CHECK_HEADERS([ev.h])
	SXE_CHECK_LIB_FUNCS([ev], [ev_loop_new])

	if test "$ac_cv_header_ev_h" = "yes" -a \
		"$ac_cv_lib_ev___ev_loop_new" = "yes"; then
		AC_DEFINE([HAVE_LIBEV], [1], [Whether libev is fully functional])
		sxe_cv_feat_libev="yes"
		have_libev="yes"
		LIBEV_CPPFLAGS=
		LIBEV_LDFLAGS=
		LIBEV_LIBS="-lev"
	else
		sxe_cv_feat_libev="no"
		have_libev="no"
		LIBEV_CPPFLAGS=
		LIBEV_LDFLAGS=
		LIBEV_LIBS=
	fi

	AC_SUBST([LIBEV_CPPFLAGS])
	AC_SUBST([LIBEV_LDFLAGS])
	AC_SUBST([LIBEV_LIBS])
])dnl SXE_CHECK_LIBEVENT

AC_DEFUN([SXE_CHECK_EVENTS], [dnl
	SXE_MSG_CHECKING([for event drivers])

	SXE_CHECK_LIBEVENT
	SXE_CHECK_SUFFICIENCY([libevent], [libevent])

	## assume we havent got any of prerequisites
	have_events="no"

	have_events="yes"

	SXE_MSG_RESULT([$have_events])
])dnl SXE_CHECK_EVENTS

dnl sxe-events.m4 ends here
