### Makefile.am

AM_CPPFLAGS =
AM_LDFLAGS =

bin_PROGRAMS = unserding
unserding_LDADD = -lev

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
