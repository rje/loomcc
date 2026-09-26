// loomcc-do: preprocess
// A hex literal too big for intmax_t is unsigned; decimal ones never are.
#if 0x8000000000000000 > 0
a
#endif
#if 18446744073709551615u == -1
b
#endif
// loomcc-expect: a b
