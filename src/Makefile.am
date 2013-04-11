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

## stuff that really does not belong here, expect this to be kicked
EXTRA_DIST += btea.c

bin_PROGRAMS += unsermon
unsermon_SOURCES = unsermon.c unsermon.h unsermon-clo.ggo
pkginclude_HEADERS += unsermon.h
unsermon_SOURCES += ud-logger.c ud-logger.h
unsermon_SOURCES += ud-module.c ud-module.h
unsermon_SOURCES += ud-nifty.h
unsermon_CPPFLAGS = $(AM_CPPFLAGS)
unsermon_CPPFLAGS += $(libev_CFLAGS)
unsermon_CPPFLAGS += $(uterus_CFLAGS)
unsermon_CPPFLAGS += $(LTDLINCL)
unsermon_LDFLAGS = $(AM_LDFLAGS) -lrt
unsermon_LDFLAGS += $(libev_LIBS)
unsermon_LDFLAGS += $(uterus_LIBS)
unsermon_LDFLAGS += $(LIBLTDL) $(LIBADD_DL) $(LD_EXPORT_DYNAMIC)
unsermon_LDADD = $(unserding_LIBS)
unsermon_DEPENDENCIES = $(LTDLDEPS)
BUILT_SOURCES += unsermon-clo.c unsermon-clo.h

bin_PROGRAMS += ud-ping
ud_ping_SOURCES = ud-ping.c ud-ping-clo.ggo
ud_ping_SOURCES += svc-pong.h
ud_ping_SOURCES += ud-nifty.h
ud_ping_SOURCES += ud-time.h
ud_ping_CPPFLAGS = $(AM_CPPFLAGS)
ud_ping_CPPFLAGS += $(libev_CFLAGS)
ud_ping_LDFLAGS = $(AM_LDFLAGS) -static
ud_ping_LDFLAGS += -lrt
ud_ping_LDFLAGS += $(libev_LIBS)
ud_ping_LDADD = $(unserding_LIBS)
BUILT_SOURCES += ud-ping-clo.c ud-ping-clo.h

bin_PROGRAMS += ud-router
ud_router_SOURCES = ud-router.c ud-router-clo.ggo
ud_router_SOURCES += daemonise.c daemonise.h
ud_router_CPPFLAGS = $(AM_CPPFLAGS)
ud_router_CPPFLAGS += $(libev_CFLAGS)
ud_router_LDFLAGS = $(AM_LDFLAGS) -static
ud_router_LDFLAGS += $(libev_LIBS)
ud_router_LDADD = libunserding.la
BUILT_SOURCES += ud-router-clo.c ud-router-clo.h

bin_PROGRAMS += ud-dealer
ud_dealer_SOURCES = ud-dealer.c ud-dealer-clo.ggo
ud_dealer_SOURCES += daemonise.c daemonise.h
ud_dealer_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
ud_dealer_CPPFLAGS += $(libev_CFLAGS)
ud_dealer_LDFLAGS = $(AM_LDFLAGS) -static
ud_dealer_LDFLAGS += $(libev_LIBS)
ud_dealer_LDADD = libunserding.la
BUILT_SOURCES += ud-dealer-clo.c ud-dealer-clo.h

## the lib, or its archive counterpart
lib_LTLIBRARIES += libunserding.la
libunserding_la_SOURCES = unserding.c
pkginclude_HEADERS += unserding.h
pkginclude_HEADERS += ud-sockaddr.h
libunserding_la_SOURCES += ud-private.h
libunserding_la_SOURCES += ud-nifty.h
libunserding_la_SOURCES += ud-sock.h
libunserding_la_SOURCES += boobs.h
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
svc_pong_la_SOURCES += ud-nifty.h
svc_pong_la_CPPFLAGS = $(AM_CPPFLAGS) -DUNSERMON_DSO
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
