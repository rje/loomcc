// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-if-eager] 816-tcc diagnoses division by zero in unevaluated #if operands
// loomcc-no-warnings
// Unevaluated operands may divide by zero.
#if 1 || 1/0
a
#endif
#if 0 && 1/0
#else
b
#endif
#if 1 ? 2 : 1/0
c
#endif
#if 0 ? 1/0 : 3
d
#endif
// loomcc-expect: a b c d
