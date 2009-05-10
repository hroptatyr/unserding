### Makefile.am

AM_CPPFLAGS = -pthread
AM_LDFLAGS = -lpthread

headerdir = $(includedir)/unserding

bin_PROGRAMS = unserdingd unsercli
noinst_PROGRAMS = e123ify
lib_LTLIBRARIES = libunserding.la
BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)

header_HEADERS = unserding.h catalogue.h protocore.h
noinst_HEADERS = arrqueue.h protocore-private.h

## the main server binary
unserdingd_SOURCES = unserdingd.c mcast4.c protocore.c arrqueue.c
unserdingd_SOURCES +=
unserdingd_CPPFLAGS = -DUNSERSRV $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
unserdingd_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS) -lrt
unserdingd_LDADD = $(LIBEV_LIBS)

## the main server binary
unsercli_SOURCES = unsercli.c stdin.c
unsercli_SOURCES += scanner.l parser.y
unsercli_SOURCES += protocore.c
unsercli_LFLAGS = --prefix="cli_yy"
unsercli_YFLAGS = -d -p "cli_yy"
unsercli_CPPFLAGS = -DUNSERCLI $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS) $(LIBEDIT_CPPFLAGS)
unsercli_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unsercli_LDADD = $(LIBEV_LIBS) -lreadline libunserding.la
BUILT_SOURCES += unsercli-parser.h unsercli-scanner.h
## weird, must be an automake bug
CLEANFILES += unsercli-parser.c unsercli-scanner.c

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

## the lib
libunserding_la_SOURCES = libstuff.c protocore.c
libunserding_la_CPPFLAGS = -DUNSERLIB
libunserding_la_LDFLAGS = $(AM_LDFLAGS)

## kick
#libtest_SOURCES = libtest.c
#libtest_CPPFLAGS = $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
#libtest_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS) -lrt
#libtest_LDADD = $(LIBEV_LIBS) libunserding.la

## example app to access unserding's E.123 service
e123ify_SOURCES = e123ify.c
e123ify_CPPFLAGS = $(AM_CPPFLAGS)
e123ify_LDFLAGS = $(AM_LDFLAGS)
e123ify_LDADD = libunserding.la -lpopt

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
