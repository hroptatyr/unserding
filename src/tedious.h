/* turn off tedious warnings */
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:869)
# pragma warning (disable:1419)
# pragma warning (disable:2259)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch-default"
# pragma GCC diagnostic ignored "-Wsign-compare"
# pragma GCC diagnostic ignored "-Wunused-parameter"
#endif	/* __GNUC__ || __INTEL_COMPILER */
