// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-if-arith] 816-tcc evaluates #if in its own int types, not intmax_t/uintmax_t
// -1 converts to UINTMAX_MAX when compared with an unsigned operand.
#if -1 > 0u
a
#endif
#if 0xffffffffffffffff == -1
b
#endif
#if (0u - 1) >> 63 == 1
c
#endif
#if -1 < 0
d
#endif
// loomcc-expect: a b c d
