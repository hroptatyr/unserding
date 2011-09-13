dnl sxe-libtool.m4 -- just a quick libtoolish macros
dnl
dnl Copyright (C) 2007, 2008 Sebastian Freundt.
dnl
dnl This file is part of SXEmacs

AC_DEFUN([SXE_CHECK_LIBTOOL], [dnl
	AC_MSG_RESULT([starting libtool investigation...])
	LT_PREREQ([2.1])
	LT_INIT([dlopen])

	LT_LIB_DLLOAD
	LT_LIB_M
	LT_SYS_DLOPEN_DEPLIBS
	LT_SYS_DLSEARCH_PATH
	LT_SYS_MODULE_EXT
	LT_SYS_MODULE_PATH
	LT_SYS_SYMBOL_USCORE
	LT_FUNC_DLSYM_USCORE

	dnl Configure libltdl
	dnl newer libtool2s will do this implicitly, we drop all support
	dnl for the `old' libtool2 stuff as this is available through
	dnl cvs only and we stick with the latest
	dnl AC_CONFIG_SUBDIRS([libltdl])
	AC_CONFIG_MACRO_DIR([libltdl/m4])

	if test -n "$export_dynamic_flag_spec"; then
		sxe_cv_export_dynamic=$(\
			echo $(eval echo "$export_dynamic_flag_spec"))
		SXE_APPEND([$sxe_cv_export_dynamic], [LDFLAGS])
	else
		AC_MSG_NOTICE([
Neither -export-dynamic nor equivalent flags are supported by your linker.
Emodules however will reference some symbols dynamically.
We assume that your linker will do what we need, but this assumption
might be wrong as well.
])dnl AC_MSG_NOTICE
	fi

	## cope with libtool's convenience lib/bin concept
	if test -n "$lt_cv_objdir"; then
		## this variable is a #define, too
		LT_OBJDIR="$lt_cv_objdir"
	else
		## hm, probably not the best idea but let's try
		LT_OBJDIR="."
	fi
	## definitely subst that though
	AC_SUBST([LT_OBJDIR])

	## currently there's no official variable for that, but `lt-'
	## seems to be a consistent choice throughout all libtools
	LT_CONVENIENCE_PREFIX="lt-"
	AC_SUBST([LT_CONVENIENCE_PREFIX])
])dnl SXE_CHECK_LIBTOOL

AC_DEFUN([SXE_CHECK_LIBLTDL], [dnl
	## make sure the libtool stuff has been run before
	AC_REQUIRE([SXE_CHECK_LIBTOOL])

	LT_CONFIG_LTDL_DIR([libltdl], [recursive])
	LTDL_INIT

	AM_CONDITIONAL([DESCEND_LIBLTDL], [test "$with_included_ltdl" = "yes"])
])dnl SXE_CHECK_LIBLTDL

dnl sxe-libtool.m4 ends here
