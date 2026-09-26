// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-if-arith] 816-tcc evaluates #if in its own int types, not intmax_t/uintmax_t
// loomcc-note: >> of a negative value is implementation-defined; loomcc,
// loomcc-note: gcc and clang all shift arithmetically.
#if (1 << 62) > 0 && (-1 >> 1) == -1 && (1u << 63) >> 63 == 1 && (-8 >> 2) == -2
yes
#endif
// loomcc-expect: yes
