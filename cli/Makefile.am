

### Makefile.am

AM_CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
AM_CPPFLAGS += -I$(top_srcdir)/src
AM_CFLAGS = -pthread
AM_LDFLAGS = -lpthread

if BUILD_CLIAPPS
bin_PROGRAMS = ud-ping
noinst_PROGRAMS = ud-itan
endif

BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)

if BUILD_LIBRARY
UD_LIBS = $(top_builddir)/src/libunserding.la
else
UD_LIBS = $(top_builddir)/src/libunserding.a
endif
UD_LDXFLAGS = -lrt

if BUILD_CLIAPPS
ud_ping_SOURCES = ud-ping.c
ud_ping_CPPFLAGS = $(AM_CPPFLAGS) 
ud_ping_LDFLAGS = $(AM_LDFLAGS) -static
ud_ping_LDADD = $(UD_LIBS)
ud_ping_LDADD += $(UD_LDXFLAGS)

ud_itan_SOURCES = ud-itan.c
ud_itan_CPPFLAGS = $(AM_CPPFLAGS)
ud_itan_LDFLAGS = $(AM_LDFLAGS) -static
ud_itan_LDADD = $(UD_LIBS)
ud_itan_LDADD += $(UD_LDXFLAGS)
endif

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
