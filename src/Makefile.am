

### Makefile.am

AM_CPPFLAGS = -pthread -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS = -lpthread

headerdir = $(includedir)/unserding
moddir = $(libdir)/unserding

bin_PROGRAMS =
if BUILD_SERVER
bin_PROGRAMS += unserdingd unsercli
noinst_PROGRAMS = instrumenstruation uteseries
endif
if BUILD_LIBRARY
lib_LTLIBRARIES = libunserding.la
endif

BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)

header_HEADERS = unserding.h protocore.h seria.h mcast.h dccp.h
noinst_HEADERS = protocore-private.h

if BUILD_SERVER
## the main server binary
unserdingd_SOURCES = unserdingd.c mcast.c dccp.c
unserdingd_SOURCES += protocore.c seria.c
unserdingd_SOURCES += module.h module.c
unserdingd_SOURCES += wpool.h wpool.c
unserdingd_SOURCES += jpool.h jpool.c
unserdingd_SOURCES += lua-config.c
unserdingd_CPPFLAGS = -DUNSERSRV $(AM_CPPFLAGS)
unserdingd_CPPFLAGS += $(LIBEV_CPPFLAGS)
unserdingd_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS) -lrt -lpopt
unserdingd_LDFLAGS += $(shell pkg-config --libs libxml-2.0)
unserdingd_LDFLAGS += -llua ## check for me
unserdingd_LDADD = $(LIBEV_LIBS) $(LIBLTDL) -lunserding

## server static modules
unserdingd_SOURCES += dso-pong.c

## server dynamic modules
mod_LTLIBRARIES =
mod_LTLIBRARIES += dso-cli.la dso-scscp.la dso-scscp2.la dso-bbdb.la dso-xbbdb.la
#mod_LTLIBRARIES += dso-instr.la
mod_LTLIBRARIES += dso-xdr-instr.la
mod_LTLIBRARIES += dso-tseries.la dso-tseries-ute.la
if USE_MYSQL
mod_LTLIBRARIES += dso-tseries-mysql.la
endif
dso_cli_la_LDFLAGS = -module
dso_scscp_la_LDFLAGS = -module
dso_scscp2_la_LDFLAGS = -module
dso_bbdb_la_LDFLAGS = -module
## the xml bbdb module
dso_xbbdb_la_CPPFLAGS = $(shell pkg-config --cflags libxml-2.0)
dso_xbbdb_la_LDFLAGS = -module $(shell pkg-config --libs libxml-2.0)

## the catalogue module
dso_xdr_instr_la_LDFLAGS = -module $(PFACK_LIBS)
dso_xdr_instr_la_CPPFLAGS = $(PFACK_CFLAGS)
dso_xdr_instr_la_LIBADD = $(PFACK_LIBS)
if USE_MYSQL
dso_xdr_instr_la_CPPFLAGS += $(MYSQL_CPPFLAGS)
dso_xdr_instr_la_LDFLAGS += $(MYSQL_LDFLAGS) -lmysqlclient
endif
dso_xdr_instr_la_SOURCES = catalogue.c dso-xdr-instr.c
if USE_MYSQL
dso_xdr_instr_la_SOURCES += dso-xdr-instr-mysql.c
endif
dso_xdr_instr_la_SOURCES += dso-xdr-instr-file.c

## the time series module
dso_tseries_la_LDFLAGS = -module $(PFACK_LIBS) -export-dynamic
dso_tseries_la_CPPFLAGS = $(AM_CPPFLAGS) $(PFACK_CFLAGS)
dso_tseries_la_LIBADD = $(PFACK_LIBS)
dso_tseries_la_SOURCES = dso-tseries.c
dso_tseries_la_SOURCES += intvtree.c tscube.c
dso_tseries_la_SOURCES += dso-tseries-frobq.c
dso_tseries_la_LIBADD += -luterus

if USE_MYSQL
dso_tseries_mysql_la_SOURCES = dso-tseries-mysql.c
dso_tseries_mysql_la_CPPFLAGS = $(MYSQL_CPPFLAGS)
dso_tseries_mysql_la_LDFLAGS = -module
dso_tseries_mysql_la_LDFLAGS += $(MYSQL_LDFLAGS) -lmysqlclient
endif

dso_tseries_ute_la_LDFLAGS = -module
dso_tseries_ute_la_SOURCES = dso-tseries-ute.c
dso_tseries_ute_la_LIBADD = -luterus

## stuff we develop here but that will go eventually to sushi
uteseries_CPPFLAGS = $(AM_CPPFLAGS) -DTEST_MODE
uteseries_LDFLAGS = -static
uteseries_LDADD = -luterus -lpopt

## to fiddle with option chains and other instruments
instrumenstruation_CPPFLAGS = $(PFACK_CFLAGS)
instrumenstruation_LDFLAGS = $(PFACK_LIBS)
instrumenstruation_LDADD = $(PFACK_LIBS) libunserding.la -lpopt
endif  ## BUILD_SERVER
header_HEADERS += xdr-instr.h tseries.h

## fucking automake
unsercli_YFLAGS = -d -p "cli_yy"
if BUILD_SERVER
## the main client binary
unsercli_SOURCES = unsercli.y scanner.l
unsercli_SOURCES += stdin.c stdin.h protocore.c cli-common.h tedious.h
unsercli_LFLAGS = --prefix="cli_yy"
unsercli_CPPFLAGS = -DUNSERCLI $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS) $(LIBEDIT_CPPFLAGS)
unsercli_CPPFLAGS += -imacros tedious.h
unsercli_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS) -lpopt
unsercli_LDADD = $(LIBEV_LIBS) $(LIBREADLINE_LIBS) libunserding.la
BUILT_SOURCES += unsercli-unsercli.h unsercli-scanner.h
## weird, must be an automake bug
CLEANFILES += unsercli-unsercli.c unsercli-scanner.c

## fucking automake refuses to replay my flex patch
unsercli-scanner.h: scanner.l
	$(LEX) $(unsercli_LFLAGS) --header-file=$@ $<
	test -e lex.yy.c && $(RM) lex.yy.c
	test -e lex.yy.h && mv lex.yy.h $@

## gperf baloney
#GPERF = gperf
#GPERFFLAGS = --multiple-iterations=8 --language=ANSI-C
#BUILT_SOURCES += keywords.c
#CLEANFILES += keywords.c keywords.c.tmp
### gperf target
#keywords.c: keywords.perf
#	if $(GPERF) $(GPERFFLAGS) $< > $@.tmp; then \
#		mv $@.tmp $@; \
#	elif $(GPERF) --version >/dev/null 2>&1; then \
#		rm $@.tmp; \
#		exit 1; \
#	else \
#		rm $@.tmp; \
#		touch $@; \
#	fi
endif

if BUILD_LIBRARY
## the lib
libunserding_la_SOURCES = libstuff.c libhelpers.c dccp.c
libunserding_la_SOURCES += svc-pong.c
libunserding_la_CPPFLAGS = -DUNSERLIB $(PFACK_CFLAGS) $(AM_CPPFLAGS)
libunserding_la_LDFLAGS = $(AM_LDFLAGS) -lrt
endif

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
