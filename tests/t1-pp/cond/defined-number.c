// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-if-lax] 816-tcc accepts malformed #if expressions
#if defined(1) // loomcc-error
#endif
