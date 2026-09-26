// loomcc-do: preprocess
// C17 6.6p3: no comma operator in an evaluated constant expression.
// loomcc-ref-diverges: tcc [tcc-if-lax] 816-tcc accepts a comma in #if
#if (1, 2) // loomcc-diagnostic: comma
#endif
