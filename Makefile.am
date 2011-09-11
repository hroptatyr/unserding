LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libltdl src cli

if BUILD_LIBRARY
pkgconfigdir = @libdir@/pkgconfig
pkgconfig_DATA = unserding.pc
endif

EXTRA_DIST = git-version-gen
