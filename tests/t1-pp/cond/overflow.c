// loomcc-do: preprocess
// C17 6.6p4: a constant expression must be representable; this overflows intmax_t.
// loomcc-ref-diverges: tcc [tcc-if-lax] 816-tcc does not diagnose #if overflow
#if 0x7fffffffffffffff + 1 // loomcc-diagnostic: overflow
#endif
