dnl ffff.m4 -- detect libffff
dnl
dnl Copyright (C) 2008 Sebastian Freundt.
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
dnl This file is part of libffff

AC_DEFUN([FFFF_CHECK_HEADERS], [dnl
	## assumes libffff_cflags is defined
	SXE_DUMP_LIBS
	CPPFLAGS="$libffff_cppflags $CPPFLAGS"
	SXE_CHECK_HEADERS([ffff/ffff.h])
	SXE_RESTORE_LIBS
])dnl FFFF_CHECK_HEADERS

AC_DEFUN([AC_CHECK_FFFF], [dnl
	## assumes $PKG_CONFIG is defined
	## arg #1: action on success
	## arg #2: action on failure
	pushdef([SUCC], [$1])
	pushdef([FAIL], [$2])

	if test -z "$PKG_CONFIG"; then
		SXE_SEARCH_CONFIG_PROG([pkg-config])
	fi

	SXE_PC_CHECK_VERSION_ATLEAST([libffff], [0.0.99])
	SXE_PC_CHECK_LIBS([libffff])
	SXE_PC_CHECK_LDFLAGS([libffff])
	SXE_PC_CHECK_CPPFLAGS([libffff])
	if test "$sxe_cv_pc_libffff_recent_enough_p" = "yes"; then
		libffff_cppflags="$sxe_cv_pc_libffff_cppflags"
		FFFF_CHECK_HEADERS
	fi

	dnl final check
	SXE_MSG_CHECKING([whether libffff provides what we need])
	if test "$ac_cv_header_ffff_ffff_h" = "yes"; then
		have_libffff="yes"
		LIBFFFF_CPPFLAGS="$sxe_cv_pc_libffff_cppflags"
		LIBFFFF_LIBS="$sxe_cv_pc_libffff_libs"
		LIBFFFF_LDFLAGS="$sxe_cv_pc_libffff_ldflags"
		SUCC
	else
		have_libffff="no"
		LIBFFFF_CPPFLAGS=
		LIBFFFF_LDFLAGS=
		LIBFFFF_LIBS=
		FAIL
	fi
	SXE_MSG_RESULT([$have_libffff])
	AC_SUBST([LIBFFFF_CPPFLAGS])
	AC_SUBST([LIBFFFF_LDFLAGS])
	AC_SUBST([LIBFFFF_LIBS])
	popdef([FAIL])
	popdef([SUCC])
])dnl CHECK_FFFF

dnl ffff.m4 ends here
