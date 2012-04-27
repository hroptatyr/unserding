LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
if DESCEND_LIBLTDL
SUBDIRS += libltdl
endif
SUBDIRS += src cli info

if BUILD_LIBRARY
pkgconfigdir = @libdir@/pkgconfig
pkgconfig_DATA = unserding.pc
endif

EXTRA_DIST = git-version-gen
