### Makefile.am

AM_CPPFLAGS = -pthread
AM_LDFLAGS = -lpthread

bin_PROGRAMS = unserdingd unsercli unsermon testcli
if USE_MYSQL
noinst_LIBRARIES = libpfack-mysql.a
endif
lib_LTLIBRARIES = libunserding.la

if USE_MYSQL
## additional helper for mysql sources
libpfack_mysql_a_SOURCES = pfack-mysql.c
libpfack_mysql_a_CPPFLAGS = $(AM_CPPFLAGS) $(MYSQL_CPPFLAGS) $(FPIC)
endif

## the main server binary
unserdingd_SOURCES = unserdingd.c mcast4.c protocore.c catalogue.c instr.c
unserdingd_CPPFLAGS = -DUNSERSRV $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
unserdingd_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unserdingd_LDADD = $(LIBEV_LIBS)
if USE_MYSQL
unserdingd_LDADD += libpfack-mysql.a
unserdingd_LDFLAGS += $(MYSQL_LDFLAGS)
## additional modules
unserdingd_SOURCES += mod-interests.c
endif

## the main server binary
unsercli_SOURCES = unsercli.c stdin.c mcast4.c proto.c protocore.c catalogue.c
unsercli_CPPFLAGS = -DUNSERCLI $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
unsercli_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unsercli_LDADD = $(LIBEV_LIBS) -lreadline

## the network monitor
unsermon_SOURCES = unsermon.c mcast4.c protocore.c catalogue.c
unsermon_CPPFLAGS = -DUNSERMON $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
unsermon_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unsermon_LDADD = $(LIBEV_LIBS)

## the lib
libunserding_la_SOURCES = libstuff.c

## the main server binary
testcli_SOURCES = testcli.c
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
