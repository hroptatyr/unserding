### Makefile.am

AM_CPPFLAGS = -pthread
AM_LDFLAGS = -lpthread

bin_PROGRAMS = unserding

## the main server binary
unserding_SOURCES = unserding.c unserding-tcp6.c proto.c catalogue.c instr.c
unserding_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unserding_LDADD = $(LIBEV_LIBS)

unserding_SOURCES += mod-interests.c

noinst_LIBRARIES =
noinst_LIBRARIES += libpfack-mysql.a
libpfack_mysql_a_SOURCES = pfack-mysql.c
libpfack_mysql_a_CPPFLAGS = $(AM_CPPFLAGS) $(MYSQL_CPPFLAGS) $(FPIC)

unserding_LDADD += libpfack-mysql.a
unserding_LDFLAGS += $(MYSQL_LDFLAGS)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
