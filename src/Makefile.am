### Makefile.am

AM_CPPFLAGS = -pthread -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS = -lpthread

moddir = $(libdir)/unserding

noinst_PROGRAMS =
noinst_HEADERS =
if BUILD_SERVER
sbin_PROGRAMS = unserdingd
endif
if BUILD_CLIAPPS
bin_PROGRAMS = unsercli
endif
if BUILD_LIBRARY
lib_LTLIBRARIES = libunserding.la
else
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
unserdingd_SOURCES = unserdingd.c mcast.c dccp.c tcp-unix.c
unserdingd_SOURCES += protocore.c seria.c
unserdingd_SOURCES += module.h module.c
unserdingd_SOURCES += wpool.h wpool.c fsarrqueue.h
unserdingd_SOURCES += jpool.h jpool.c
unserdingd_SOURCES += lua-config.c
unserdingd_CPPFLAGS = -DUNSERSRV $(AM_CPPFLAGS)
unserdingd_CPPFLAGS += $(LUA_CFLAGS)
unserdingd_CPPFLAGS += $(LIBEV_CPPFLAGS)
unserdingd_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unserdingd_LDFLAGS += $(LUA_LIBS)
unserdingd_LDFLAGS += $(LIBEV_LIBS)
unserdingd_LDFLAGS += $(LIBLTDL)
if BUILD_LIBRARY
unserdingd_LDADD = libunserding.la
else
unserdingd_LDADD = libunserding.a
endif
unserdingd_LDADD += -lrt
BUILT_SOURCES += unserdingd-clo.c unserdingd-clo.h

## server static modules
unserdingd_SOURCES += dso-pong.c
endif

## server dynamic modules
mod_LTLIBRARIES =

## fucking automake
unsercli_YFLAGS = -d -p "cli_yy"
if BUILD_CLIAPPS
## the main client binary
unsercli_SOURCES = unsercli.y scanner.l
unsercli_SOURCES += stdin.c stdin.h protocore.c cli-common.h tedious.h
unsercli_LFLAGS = --prefix="cli_yy"
unsercli_CPPFLAGS = -DUNSERCLI $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS) $(LIBEDIT_CPPFLAGS)
unsercli_CPPFLAGS += -imacros tedious.h
unsercli_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unsercli_LDFLAGS += $(LIBEV_LIBS)
unsercli_LDFLAGS += $(LIBREADLINE_LIBS)
if BUILD_LIBRARY
unsercli_LDADD = libunserding.la
else
unsercli_LDADD = libunserding.a
endif
unsercli_LDADD += -lrt
endif
BUILT_SOURCES += unsercli-unsercli.h unsercli-scanner.h
BUILT_SOURCES += unsercli-clo.c unsercli-clo.h

## fucking automake refuses to replay my flex patch
unsercli-scanner.h: scanner.l
	$(LEX) $(unsercli_LFLAGS) --header-file=$@ $<
	test -e lex.yy.c && $(RM) lex.yy.c
	test -e lex.yy.h && mv lex.yy.h $@

## the lib, or its archive counterpart
if BUILD_LIBRARY
libunserding_la_SOURCES = libstuff.c dccp.c
libunserding_la_SOURCES += svc-pong.c svc-pong.h
libunserding_la_CPPFLAGS = -DUNSERLIB $(AM_CPPFLAGS)
libunserding_la_LDFLAGS = $(AM_LDFLAGS)
libunserding_la_LIBADD = -lrt
else
libunserding_a_SOURCES = libstuff.c dccp.c
libunserding_a_SOURCES += svc-pong.c svc-pong.h
libunserding_a_CPPFLAGS = $(AM_CPPFLAGS) -DUNSERLIB
endif

## our rule for gengetopt
%.c %.h: %.ggo
	gengetopt -l -i $< -F $*

#
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
