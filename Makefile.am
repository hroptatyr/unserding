LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
if DESCEND_LIBLTDL
SUBDIRS += libltdl
endif  DESCEND_LIBLTDL
SUBDIRS += src info test

pkgconfigdir = @libdir@/pkgconfig
pkgconfig_DATA = unserding.pc

EXTRA_DIST = git-version-gen
