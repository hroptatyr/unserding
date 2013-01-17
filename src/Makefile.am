### Makefile.am

AM_CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
AM_CFLAGS = -pthread
AM_LDFLAGS = -lpthread

moddir = $(libdir)/unserding

noinst_PROGRAMS =
noinst_HEADERS =
bin_PROGRAMS =
sbin_PROGRAMS =
lib_LTLIBRARIES = libunserding.la
unserding_LIBS = libunserding.la

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

headerdir = $(includedir)/unserding
header_HEADERS =
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

## the main server binary
sbin_PROGRAMS += unserdingd
unserdingd_SOURCES = unserdingd.c mcast.c dccp.c tcp-unix.c
unserdingd_SOURCES += protocore.c seria.c
unserdingd_SOURCES += module.h module.c
unserdingd_SOURCES += wpool.h wpool.c fsarrqueue.h
unserdingd_SOURCES += jpool.h jpool.c
unserdingd_SOURCES += lua-config.c
unserdingd_CPPFLAGS = -DUNSERSRV $(AM_CPPFLAGS)
unserdingd_CPPFLAGS += $(LUA_CFLAGS)
unserdingd_CPPFLAGS += $(LIBEV_CFLAGS)
unserdingd_LDFLAGS = $(AM_LDFLAGS)
unserdingd_LDFLAGS += $(LUA_LIBS)
unserdingd_LDFLAGS += $(LIBEV_LIBS)
unserdingd_LDFLAGS += $(LIBLTDL)
unserdingd_LDADD = $(unserding_LIBS)
unserdingd_LDADD += -lrt
BUILT_SOURCES += unserdingd-clo.c unserdingd-clo.h

## server static modules
unserdingd_SOURCES += dso-pong.c

## server dynamic modules
mod_LTLIBRARIES =

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

## the lib, or its archive counterpart
libunserding_la_SOURCES = libstuff.c dccp.c
libunserding_la_SOURCES += svc-pong.c svc-pong.h
libunserding_la_SOURCES += mcast.c mcast.h
libunserding_la_CPPFLAGS = -DUNSERLIB $(AM_CPPFLAGS)
libunserding_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS)

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
