### Makefile.am

AM_CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
AM_CFLAGS =
AM_LDFLAGS =
AM_MODFLAGS = -module -shared -avoid-version

noinst_PROGRAMS =
bin_PROGRAMS =
sbin_PROGRAMS =
lib_LTLIBRARIES =
noinst_HEADERS =
pkginclude_HEADERS =
unsermod_LTLIBRARIES =

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

noinst_HEADERS += ud-sock.h
noinst_HEADERS += unserding-nifty.h unserding-dbg.h
noinst_HEADERS += boobs.h
## stuff that really does not belong here, expect this to be kicked
EXTRA_DIST += btea.c

bin_PROGRAMS += unsermon
unsermon_SOURCES = unsermon.c unsermon.h unsermon-clo.ggo
pkginclude_HEADERS += unsermon.h
unsermon_SOURCES += ud-logger.c ud-logger.h
unsermon_SOURCES += ud-module.c ud-module.h
unsermon_CPPFLAGS = $(AM_CPPFLAGS)
unsermon_CPPFLAGS += $(libev_CFLAGS)
unsermon_CPPFLAGS += $(uterus_CFLAGS)
unsermon_CPPFLAGS += $(INCLTDL)
unsermon_LDFLAGS = $(AM_LDFLAGS) -lrt
unsermon_LDFLAGS += $(libev_LIBS)
unsermon_LDFLAGS += $(uterus_LIBS)
unsermon_LDFLAGS += $(LIBLTDL) $(LIBADD_DL) $(LD_EXPORT_DYNAMIC)
unsermon_LDADD = $(unserding_LIBS)
BUILT_SOURCES += unsermon-clo.c unsermon-clo.h

bin_PROGRAMS += ud-ping
ud_ping_SOURCES = ud-ping.c ud-ping-clo.ggo
ud_ping_SOURCES += svc-pong.h
ud_ping_SOURCES += ud-time.h
ud_ping_CPPFLAGS = $(AM_CPPFLAGS)
ud_ping_CPPFLAGS += $(libev_CFLAGS)
ud_ping_LDFLAGS = $(AM_LDFLAGS) -static
ud_ping_LDFLAGS += -lrt
ud_ping_LDFLAGS += $(libev_LIBS)
ud_ping_LDADD = $(unserding_LIBS)
BUILT_SOURCES += ud-ping-clo.c ud-ping-clo.h

## the lib, or its archive counterpart
lib_LTLIBRARIES += libunserding.la
libunserding_la_SOURCES = unserding.c
pkginclude_HEADERS += unserding.h
pkginclude_HEADERS += ud-sockaddr.h
libunserding_la_SOURCES += ud-private.h
libunserding_la_SOURCES += svc-pong.c svc-pong.h
libunserding_la_SOURCES += ud-logger.c ud-logger.h
libunserding_la_CPPFLAGS = -DUNSERLIB $(AM_CPPFLAGS)
libunserding_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS)
libunserding_la_LDFLAGS += -version-info 2:0:0
libunserding_la_LDFLAGS += $(LD_EXPORT_DYNAMIC)
unserding_LIBS = libunserding.la

## ping/pong decoder for unsermon
unsermod_LTLIBRARIES += svc-pong.la
svc_pong_la_SOURCES = svc-pong.c svc-pong.h
svc_pong_la_CPPFLAGS = $(AM_CPPFLAGS) -DUNSERMON_DSO
svc_pong_la_CPPFLAGS += -DUD_NEW_API
svc_pong_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS) $(AM_MODFLAGS)


## our rule for gengetopt
%.c %.h: %.ggo
	$(AM_V_GEN) gengetopt -l -i $< -F $*

#
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
