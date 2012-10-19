### Makefile.am

AM_CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
AM_CFLAGS = -pthread
AM_LDFLAGS = -lpthread

moddir = $(libdir)/unserding

noinst_PROGRAMS =
noinst_HEADERS =
bin_PROGRAMS =
sbin_PROGRAMS =
if BUILD_LIBRARY
lib_LTLIBRARIES = libunserding.la
unserding_LIBS = libunserding.la
else
unserding_LIBS = libunserding.a
noinst_LIBRARIES = libunserding.a
endif

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

if BUILD_LIBRARY
headerdir = $(includedir)/unserding
header_HEADERS =
header_HEADERS += unserding.h protocore.h seria.h module.h
header_HEADERS += mcast.h dccp.h tcp-unix.h
header_HEADERS += logger.h
header_HEADERS += unserding-cfg.h
header_HEADERS += unserding-ctx.h
header_HEADERS += lua-config.h
endif
noinst_HEADERS += unserding-private.h protocore-private.h
noinst_HEADERS += ud-sock.h epoll-helpers.h ud-time.h
noinst_HEADERS += unserding-nifty.h unserding-dbg.h
noinst_HEADERS += seria-proto-glue.h
## stuff that really does not belong here, expect this to be kicked
EXTRA_DIST += svc-itanl.h
EXTRA_DIST += btea.c

if BUILD_SERVER
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
endif

## server dynamic modules
mod_LTLIBRARIES =

## fucking automake
unsercli_YFLAGS = -d
if BUILD_CLIAPPS
## the main client binary
bin_PROGRAMS += unsercli
unsercli_SOURCES = unsercli.y
unsercli_SOURCES += stdin.c stdin.h protocore.c tedious.h
unsercli_CPPFLAGS = -DUNSERCLI $(AM_CPPFLAGS) $(LIBEV_CFLAGS) $(LIBEDIT_CPPFLAGS)
unsercli_CPPFLAGS += -imacros tedious.h
unsercli_LDFLAGS = $(AM_LDFLAGS)
unsercli_LDFLAGS += $(LIBEV_LIBS)
unsercli_LDFLAGS += $(LIBREADLINE_LIBS)
unsercli_LDADD = $(unserding_LIBS)
unsercli_LDADD += -lrt
EXTRA_unsercli_SOURCES = scanner.l
endif ## BUILD_CLIAPPS
BUILT_SOURCES += scanner.c
BUILT_SOURCES += unsercli-clo.c unsercli-clo.h

if BUILD_CLIAPPS
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
endif ## BUILD_CLIAPPS
BUILT_SOURCES += unsermon-clo.c unsermon-clo.h

## the lib, or its archive counterpart
if BUILD_LIBRARY
libunserding_la_SOURCES = libstuff.c dccp.c
libunserding_la_SOURCES += svc-pong.c svc-pong.h
libunserding_la_SOURCES += mcast.c mcast.h
libunserding_la_CPPFLAGS = -DUNSERLIB $(AM_CPPFLAGS)
libunserding_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS)
else
libunserding_a_SOURCES = libstuff.c dccp.c
libunserding_a_SOURCES += svc-pong.c svc-pong.h
libunserding_a_CPPFLAGS = $(AM_CPPFLAGS) -DUNSERLIB
endif

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
