### Makefile.am

AM_CPPFLAGS = -pthread
AM_LDFLAGS = -lpthread

bin_PROGRAMS = unserdingd

## the main server binary
unserdingd_SOURCES = unserdingd.c tcp6.c proto.c catalogue.c instr.c
unserdingd_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unserdingd_LDADD = $(LIBEV_LIBS)

unserdingd_SOURCES += mod-interests.c

noinst_LIBRARIES =
noinst_LIBRARIES += libpfack-mysql.a
libpfack_mysql_a_SOURCES = pfack-mysql.c
libpfack_mysql_a_CPPFLAGS = $(AM_CPPFLAGS) $(MYSQL_CPPFLAGS) $(FPIC)

unserdingd_LDADD += libpfack-mysql.a
unserdingd_LDFLAGS += $(MYSQL_LDFLAGS)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
