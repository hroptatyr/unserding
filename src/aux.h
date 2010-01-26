/* auxiliary functions */

#if defined __x86_64__ || defined __amd64__ || defined __powerpc64__
# define T	uint64_t
#elif defined __i386__
# define T	uint32_t
#else
# error cannot provide popcnt implementation
#endif
#if defined __SSE4_2__
/* no need for extra checks, this will never be available on a i386 */
static inline size_t
__popcnt(T v)
{
	return _mm_popcnt_u64(v);
}
#else  /* !__SSE4_2__ */
/* god i crave a hardware popcnt */
static inline size_t
__popcnt(T v)
{
/* found on http://graphics.stanford.edu/~seander/bithacks.html */
	size_t c;

	v = v - ((v >> 1) & (T)~(T)0/3);
	v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);
	v = (v + (v >> 4)) & (T)~(T)0/255 * 15;
	c = (T)(v * ((T)~(T)0/255)) >> (sizeof(v) - 1) * CHAR_BIT;
	return c;
}
#endif	/* __SSE4_2__ */
#undef T
