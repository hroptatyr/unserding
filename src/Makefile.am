### Makefile.am

AM_CPPFLAGS = -pthread
AM_LDFLAGS = -lpthread

bin_PROGRAMS = unserdingd unserding
if USE_MYSQL
noinst_LIBRARIES = libpfack-mysql.a
endif

if USE_MYSQL
## additional helper for mysql sources
libpfack_mysql_a_SOURCES = pfack-mysql.c
libpfack_mysql_a_CPPFLAGS = $(AM_CPPFLAGS) $(MYSQL_CPPFLAGS) $(FPIC)
endif

## the main server binary
unserdingd_SOURCES = unserdingd.c mcast4.c protocore.c proto.c catalogue.c instr.c
unserdingd_CPPFLAGS = $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
unserdingd_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS) $(MYSQL_LDFLAGS)
unserdingd_LDADD = $(LIBEV_LIBS)
if USE_MYSQL
unserdingd_LDADD += libpfack-mysql.a
## additional modules
unserdingd_SOURCES += mod-interests.c
endif

## the main server binary
unserding_SOURCES = unserding.c stdin.c proto.c catalogue.c
unserding_CPPFLAGS = $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
unserding_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unserding_LDADD = $(LIBEV_LIBS) -lreadline

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
