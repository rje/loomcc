// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-if-arith] 816-tcc evaluates #if in its own int types, not intmax_t/uintmax_t
#if ~0 == -1 && !0 == 1 && -(-1) == 1 && +1 == 1 && !!7 == 1 && ~0u == 0xffffffffffffffff
yes
#endif
// loomcc-expect: yes
