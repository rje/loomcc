// loomcc-do: preprocess
#if 0
#if 1
a
#else
b
#endif
#elif 1
#ifdef X
c
#else
d
#endif
#endif
// loomcc-expect: d
