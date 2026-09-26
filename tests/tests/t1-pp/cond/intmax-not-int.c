// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-if-arith] 816-tcc evaluates #if in its own int types, not intmax_t/uintmax_t
// #if arithmetic is intmax_t/uintmax_t (64 bits), never the target's 16-bit int.
#if 32767 + 1 > 0 && 65535 + 1 == 65536 && 0xFFFF != -1 && 2147483647 + 1 > 0
yes
#endif
// loomcc-expect: yes
