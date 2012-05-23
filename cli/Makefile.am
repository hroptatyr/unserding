

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
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

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
ud_itan_CPPFLAGS += -D_GNU_SOURCE
ud_itan_LDFLAGS = $(AM_LDFLAGS) -static
ud_itan_LDADD = $(UD_LIBS)
ud_itan_LDADD += $(UD_LDXFLAGS)

if HAVE_UTERUS
bin_PROGRAMS += ud-xmit
ud_xmit_clo = ud-xmit-clo.c ud-xmit-clo.h
ud_xmit_SOURCES = ud-xmit.c
ud_xmit_CPPFLAGS = $(AM_CPPFLAGS)
ud_xmit_CPPFLAGS += $(uterus_CFLAGS)
ud_xmit_LDFLAGS = -static
ud_xmit_LDFLAGS += $(uterus_LIBS)
ud_xmit_LDADD = $(UD_LIBS) $(UD_LDXFLAGS)

bin_PROGRAMS += umod-example
umod_example_clo = umod-example-clo.c umod-example-clo.h
umod_example_CPPFLAGS = $(AM_CPPFLAGS)
umod_example_CPPFLAGS += $(uterus_CFLAGS)
umod_example_CPPFLAGS += $(LIBEV_CFLAGS)
umod_example_LDFLAGS = -static
umod_example_LDFLAGS += $(uterus_LIBS)
umod_example_LDFLAGS += $(LIBEV_LIBS)
umod_example_LDADD = $(UD_LIBS) $(UD_LDXFLAGS)

bin_PROGRAMS += strat-example
strat_example_clo = strat-example-clo.c strat-example-clo.h
strat_example_CPPFLAGS = $(AM_CPPFLAGS)
strat_example_CPPFLAGS += $(uterus_CFLAGS)
strat_example_CPPFLAGS += $(LIBEV_CFLAGS)
strat_example_LDFLAGS = -static
strat_example_LDFLAGS += $(uterus_LIBS)
strat_example_LDFLAGS += $(LIBEV_LIBS)
strat_example_LDADD = $(UD_LIBS) $(UD_LDXFLAGS)
endif  ## HAVE_UTERUS

bin_PROGRAMS += ud-meta
ud_meta_clo = ud-meta-clo.c ud-meta-clo.h
ud_meta_SOURCES = ud-meta.c
ud_meta_CPPFLAGS = $(AM_CPPFLAGS)
ud_meta_CPPFLAGS += $(uterus_CFLAGS)
ud_meta_LDFLAGS = -static
ud_meta_LDFLAGS += $(uterus_LIBS)
ud_meta_LDADD = $(UD_LIBS) $(UD_LDXFLAGS)
endif  ## BUILD_CLIAPPS
BUILT_SOURCES += $(ud_xmit_clo)
BUILT_SOURCES += $(ud_meta_clo)
BUILT_SOURCES += $(umod_example_clo)
BUILT_SOURCES += $(strat_example_clo)

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
