### Makefile.am

AM_CPPFLAGS = -pthread
AM_LDFLAGS = -lpthread

bin_PROGRAMS = unserding

## the main server binary
unserding_SOURCES = unserding.c unserding-tcp6.c proto.c catalogue.c instr.c
unserding_LDFLAGS = $(AM_LDFLAGS) $(LIBEV_LDFLAGS)
unserding_LDADD = $(LIBEV_LIBS)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
