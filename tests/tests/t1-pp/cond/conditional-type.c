// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-if-arith] 816-tcc evaluates #if in its own int types, not intmax_t/uintmax_t
// ?: applies the usual arithmetic conversions: (1 ? -1 : 0u) is UINTMAX_MAX.
#if (1 ? -1 : 0u) > 0
yes
#endif
#if (1 ? -1 : 0) < 0
yes2
#endif
// loomcc-expect: yes yes2
