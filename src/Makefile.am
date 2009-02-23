### Makefile.am

AM_CPPFLAGS = -pthread
AM_LDFLAGS = -lpthread

headerdir = $(includedir)/unserding

bin_PROGRAMS = unserdingd unsercli testcli
lib_LTLIBRARIES = libunserding.la
BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)

header_HEADERS = unserding.h catalogue.h protocore.h

## the main server binary
unserdingd_SOURCES = unserdingd.c mcast4.c protocore.c catalogue.c instr.c
unserdingd_CPPFLAGS = -DUNSERSRV $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
unserdingd_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unserdingd_LDADD = $(LIBEV_LIBS)
## additional modules
unserdingd_SOURCES += mod-pseu-index.c
if USE_MYSQL
unserdingd_CPPFLAGS += $(LIBPFACK_CPPFLAGS)
unserdingd_LDFLAGS += $(LIBPFACK_LDFLAGS)
unserdingd_LDADD += $(LIBPFACK_LIBS) -lpfack-mysql
## additional modules
unserdingd_SOURCES += mod-interests.c
endif

## the main server binary
unsercli_SOURCES = unsercli.c scanner.l parser.y protocore.c catalogue.c stdin.c
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

## the lib
libunserding_la_SOURCES = libstuff.c

## the main server binary
testcli_SOURCES = testcli.c catalogue.c
testcli_CPPFLAGS = $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
testcli_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
testcli_LDADD = $(LIBEV_LIBS) -lreadline libunserding.la

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
