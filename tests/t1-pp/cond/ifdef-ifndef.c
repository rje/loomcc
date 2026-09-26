// loomcc-do: preprocess
#define D
#ifdef D
a
#endif
#ifdef U
b
#endif
#ifndef U
c
#endif
#ifndef D
d
#endif
// loomcc-expect: a c
