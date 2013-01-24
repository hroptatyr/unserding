### Makefile.am

AM_CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
AM_CFLAGS =
AM_LDFLAGS =

headerdir = $(includedir)/unserding
moddir = $(libexecdir)/unserding

noinst_PROGRAMS =
bin_PROGRAMS =
sbin_PROGRAMS =
lib_LTLIBRARIES =
noinst_HEADERS =
header_HEADERS =
mod_LTLIBRARIES =

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

header_HEADERS += protocore.h seria.h
header_HEADERS += mcast.h
header_HEADERS += unserding-ctx.h
noinst_HEADERS += protocore-private.h
noinst_HEADERS += ud-sock.h epoll-helpers.h
noinst_HEADERS += unserding-nifty.h unserding-dbg.h
noinst_HEADERS += seria-proto-glue.h
noinst_HEADERS += boobs.h
## stuff that really does not belong here, expect this to be kicked
EXTRA_DIST += btea.c
EXTRA_DIST += dso-pong.c

bin_PROGRAMS += unsermon
unsermon_SOURCES = unsermon.c unsermon.h unsermon-clo.ggo
unsermon_SOURCES += ud-logger.c ud-logger.h
unsermon_CPPFLAGS = $(AM_CPPFLAGS)
unsermon_CPPFLAGS += $(libev_CFLAGS)
unsermon_CPPFLAGS += $(uterus_CFLAGS)
unsermon_CPPFLAGS += -DUD_NEW_API
unsermon_LDFLAGS = $(AM_LDFLAGS) -lrt
unsermon_LDFLAGS += $(libev_LIBS)
unsermon_LDFLAGS += $(uterus_LIBS)
unsermon_LDADD = $(unserding_LIBS)
unsermon_LDADD += svc-pong.la
if HAVE_UTERUS
unsermon_LDADD += svc-uterus.la
endif  HAVE_UTERUS
BUILT_SOURCES += unsermon-clo.c unsermon-clo.h

bin_PROGRAMS += ud-ping
ud_ping_SOURCES = ud-ping.c ud-ping-clo.ggo
ud_ping_SOURCES += svc-pong.h
ud_ping_SOURCES += ud-time.h
ud_ping_CPPFLAGS = $(AM_CPPFLAGS)
ud_ping_CPPFLAGS += $(libev_CFLAGS)
ud_ping_CPPFLAGS += -DUD_NEW_API
ud_ping_LDFLAGS = $(AM_LDFLAGS) -static
ud_ping_LDFLAGS += -lrt
ud_ping_LDFLAGS += $(libev_LIBS)
ud_ping_LDADD = $(unserding_LIBS)
BUILT_SOURCES += ud-ping-clo.c ud-ping-clo.h

## the lib, or its archive counterpart
lib_LTLIBRARIES += libunserding.la
libunserding_la_SOURCES = unserding.c
header_HEADERS += unserding.h
header_HEADERS += ud-sockaddr.h
libunserding_la_SOURCES += libstuff.c
libunserding_la_SOURCES += ud-private.h
libunserding_la_SOURCES += svc-pong.c svc-pong.h
libunserding_la_SOURCES += mcast.c mcast.h
libunserding_la_SOURCES += ud-logger.c ud-logger.h
libunserding_la_CPPFLAGS = -DUNSERLIB $(AM_CPPFLAGS)
libunserding_la_CPPFLAGS += -DUD_NEW_API
libunserding_la_CPPFLAGS += -DUD_COMPAT
libunserding_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS)
libunserding_la_LDFLAGS += -version-info 1:0:1
unserding_LIBS = libunserding.la

## ping/pong decoder for unsermon
mod_LTLIBRARIES += svc-pong.la
svc_pong_la_SOURCES = svc-pong.c
svc_pong_la_CPPFLAGS = $(AM_CPPFLAGS) -DUNSERMON_DSO
svc_pong_la_CPPFLAGS += -DUD_NEW_API
svc_pong_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS) -module

## uterus decoder for unsermon, will disappear
if HAVE_UTERUS
mod_LTLIBRARIES += svc-uterus.la
svc_uterus_la_SOURCES = svc-uterus.c
svc_uterus_la_CPPFLAGS = $(AM_CPPFLAGS) -DUNSERMON_DSO
svc_uterus_la_CPPFLAGS += -DUD_NEW_API
svc_uterus_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS) -module
endif  HAVE_UTERUS


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
