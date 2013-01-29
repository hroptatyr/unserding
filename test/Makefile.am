# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG = C
LC_ALL = C

AM_CPPFLAGS = -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=201001L
AM_LDFLAGS =

## mimick pkg-config values later on
unserding_CFLAGS = -I$(abs_top_srcdir)/src -DUD_NEW_API
unserding_LIBS = -L$(abs_top_builddir)/src -lunserding

EXTRA_DIST = $(BUILT_SOURCES)
TESTS =
BUILT_SOURCES =
check_PROGRAMS =

## test the core api
check_PROGRAMS += test_pub_01
TESTS += test_pub_01
test_pub_01_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pub_01_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pub_02
TESTS += test_pub_02
test_pub_02_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pub_02_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pub_03
TESTS += test_pub_03
test_pub_03_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pub_03_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pub_04
TESTS += test_pub_04
test_pub_04_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pub_04_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pub_05
TESTS += test_pub_05
test_pub_05_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pub_05_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pubsub_06
TESTS += test_pubsub_06
test_pubsub_06_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pubsub_06_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pubsub_07
TESTS += test_pubsub_07
test_pubsub_07_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pubsub_07_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pubsub_08
TESTS += test_pubsub_08
test_pubsub_08_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pubsub_08_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pubsub_09
TESTS += test_pubsub_09
test_pubsub_09_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pubsub_09_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pubsub_10
TESTS += test_pubsub_10
test_pubsub_10_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pubsub_10_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pubsub_11
TESTS += test_pubsub_11
test_pubsub_11_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pubsub_11_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

check_PROGRAMS += test_pubsub_12
TESTS += test_pubsub_12
test_pubsub_12_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
test_pubsub_12_LDFLAGS = $(AM_LDFLAGS) $(unserding_LIBS)

.NOTPARALLEL:

## Makefile.am ends here
