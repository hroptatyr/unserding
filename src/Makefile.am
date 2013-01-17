### Makefile.am

AM_CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
AM_CFLAGS =
AM_LDFLAGS =

headerdir = $(includedir)/unserding

noinst_PROGRAMS =
bin_PROGRAMS =
sbin_PROGRAMS =
lib_LTLIBRARIES =
noinst_HEADERS =
header_HEADERS =

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

header_HEADERS += unserding.h protocore.h seria.h module.h
header_HEADERS += mcast.h dccp.h tcp-unix.h
header_HEADERS += logger.h
header_HEADERS += unserding-cfg.h
header_HEADERS += unserding-ctx.h
header_HEADERS += lua-config.h
noinst_HEADERS += unserding-private.h protocore-private.h
noinst_HEADERS += ud-sock.h epoll-helpers.h ud-time.h
noinst_HEADERS += unserding-nifty.h unserding-dbg.h
noinst_HEADERS += seria-proto-glue.h
## stuff that really does not belong here, expect this to be kicked
EXTRA_DIST += svc-itanl.h
EXTRA_DIST += btea.c
EXTRA_DIST += dso-pong.c

bin_PROGRAMS += unsermon
unsermon_SOURCES = unsermon.c unsermon-clo.ggo
unsermon_SOURCES += mcast.c mcast.h
unsermon_SOURCES += wpool.h wpool.c fsarrqueue.h
unsermon_SOURCES += jpool.h jpool.c
unsermon_SOURCES += protocore.c protocore.h protocore-private.h
unsermon_CPPFLAGS = $(AM_CPPFLAGS)
unsermon_CPPFLAGS += $(LIBEV_CFLAGS)
unsermon_CPPFLAGS += $(uterus_CFLAGS)
unsermon_LDFLAGS = $(AM_LDFLAGS) -lrt
unsermon_LDFLAGS += $(LIBEV_LIBS)
unsermon_LDFLAGS += $(uterus_LIBS)
unsermon_LDADD = $(unserding_LIBS)
BUILT_SOURCES += unsermon-clo.c unsermon-clo.h

bin_PROGRAMS += ud-ping
ud_ping_SOURCES = ud-ping.c
ud_ping_CPPFLAGS = $(AM_CPPFLAGS)
ud_ping_LDFLAGS = $(AM_LDFLAGS) -static
ud_ping_LDADD = $(unserding_LIBS)

## the lib, or its archive counterpart
lib_LTLIBRARIES += libunserding.la
libunserding_la_SOURCES = libstuff.c dccp.c
libunserding_la_SOURCES += svc-pong.c svc-pong.h
libunserding_la_SOURCES += mcast.c mcast.h
libunserding_la_CPPFLAGS = -DUNSERLIB $(AM_CPPFLAGS)
libunserding_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS)
unserding_LIBS = libunserding.la

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
