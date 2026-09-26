// loomcc-do: preprocess
#define V 3
#if V == 1
one
#elif V == 2
two
#elif V == 3
three
#elif V == 3
three-again
#else
other
#endif
// loomcc-expect: three
