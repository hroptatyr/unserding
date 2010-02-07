

### Makefile.am

AM_CPPFLAGS = -pthread -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_CPPFLAGS += -I$(top_srcdir)/src
AM_LDFLAGS = -lpthread

bin_PROGRAMS =
if BUILD_CLIAPPS
bin_PROGRAMS += ud-instr ud-snap ud-tick ud-ping ud-test ud-itan
endif

BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)

if BUILD_CLIAPPS
ud_instr_SOURCES = ud-instr.c
ud_instr_CPPFLAGS = $(AM_CPPFLAGS) $(PFACK_CFLAGS)
ud_instr_LDADD = $(top_builddir)/src/libunserding.la $(PFACK_LIBS) -lpopt
ud_instr_LDFLAGS = $(AM_LDFLAGS) -static

ud_snap_SOURCES = ud-snap.c
ud_snap_CPPFLAGS = $(AM_CPPFLAGS) $(PFACK_CFLAGS)
ud_snap_LDADD = $(top_builddir)/src/libunserding.la $(PFACK_LIBS)
ud_snap_LDFLAGS = $(AM_LDFLAGS) -static

ud_tick_SOURCES = ud-tick.c
ud_tick_CPPFLAGS = $(AM_CPPFLAGS) $(PFACK_CFLAGS)
ud_tick_LDADD = $(top_builddir)/src/libunserding.la $(PFACK_LIBS)
ud_tick_LDFLAGS = $(AM_LDFLAGS) -static

ud_ping_SOURCES = ud-ping.c
ud_ping_CPPFLAGS = $(AM_CPPFLAGS) 
ud_ping_LDADD = $(top_builddir)/src/libunserding.la -lrt
ud_ping_LDFLAGS = $(AM_LDFLAGS) -static

ud_test_SOURCES = ud-test.c
ud_test_CPPFLAGS = $(AM_CPPFLAGS) $(PFACK_CFLAGS)
ud_test_LDADD = $(top_builddir)/src/libunserding.la $(PFACK_LIBS)
ud_test_LDFLAGS = $(AM_LDFLAGS) -static

ud_itan_SOURCES = ud-itan.c
ud_itan_CPPFLAGS = $(AM_CPPFLAGS)
ud_itan_LDADD = $(top_builddir)/src/libunserding.la
ud_itan_LDFLAGS = $(AM_LDFLAGS) -static
endif

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
