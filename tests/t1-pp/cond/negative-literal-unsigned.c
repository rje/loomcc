// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-if-arith] 816-tcc evaluates #if in its own int types, not intmax_t/uintmax_t
// -0u is 0 (unsigned); -1u is UINTMAX_MAX.
#if -0u == 0 && -1u > 0 && -1u == 0xffffffffffffffff
yes
#endif
// loomcc-expect: yes
