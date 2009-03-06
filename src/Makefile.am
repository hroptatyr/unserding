### Makefile.am

AM_CPPFLAGS = -pthread
AM_LDFLAGS = -lpthread

headerdir = $(includedir)/unserding

bin_PROGRAMS = unserdingd unsercli
lib_LTLIBRARIES = libunserding.la
BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)

header_HEADERS = unserding.h catalogue.h protocore.h

## the main server binary
unserdingd_SOURCES = unserdingd.c mcast4.c protocore.c
unserdingd_SOURCES += catalogue-ng.c
unserdingd_CPPFLAGS = -DUNSERSRV $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
unserdingd_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unserdingd_LDADD = $(LIBEV_LIBS) $(LIBPFACK_LIBS)
## additional modules
unserdingd_SOURCES += mod-pseu-index.c
if USE_MYSQL
unserdingd_CPPFLAGS += $(LIBPFACK_CPPFLAGS)
unserdingd_LDFLAGS += $(LIBPFACK_LDFLAGS)
unserdingd_LDADD += -lpfack-mysql
## additional modules
unserdingd_SOURCES += mod-interests.c
endif
## and yet more modules, 4217 (ccys)
unserdingd_SOURCES += mod-instr-fx.c

## the main server binary
unsercli_SOURCES = unsercli.c stdin.c
unsercli_SOURCES += scanner.l parser.y keywords.c
unsercli_SOURCES += protocore.c catalogue-ng.c
unsercli_LFLAGS = --prefix="cli_yy"
unsercli_YFLAGS = -d -p "cli_yy"
unsercli_CPPFLAGS = -DUNSERCLI $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
unsercli_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unsercli_LDADD = $(LIBEV_LIBS) -lreadline -lfl libunserding.la
BUILT_SOURCES += unsercli-parser.h unsercli-scanner.h
## weird, must be an automake bug
CLEANFILES += unsercli-parser.c unsercli-scanner.c

## fucking automake refuses to replay my flex patch
unsercli-scanner.h: scanner.l
	$(LEX) $(unsercli_LFLAGS) --header-file=$@ $<
	test -e lex.yy.c && $(RM) lex.yy.c
	test -e lex.yy.h && mv lex.yy.h $@

## gperf baloney
GPERF = gperf
GPERFFLAGS = --multiple-iterations=8 --language=ANSI-C
BUILT_SOURCES += keywords.c
CLEANFILES += keywords.c keywords.c.tmp
## gperf target
keywords.c: keywords.perf
	if $(GPERF) $(GPERFFLAGS) $< > $@.tmp; then \
		mv $@.tmp $@; \
	elif $(GPERF) --version >/dev/null 2>&1; then \
		rm $@.tmp; \
		exit 1; \
	else \
		rm $@.tmp; \
		touch $@; \
	fi

## the lib
libunserding_la_SOURCES = libstuff.c

## kick
#testcli_SOURCES = testcli.c
#testcli_CPPFLAGS = $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
#testcli_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
#testcli_LDADD = $(LIBEV_LIBS) -lreadline libunserding.la

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
