// loomcc-do: syntax
// loomcc-note: 6.9.1p6: every identifier in the list must be declared (no
// loomcc-note: implicit int since C99). Undefined, not a constraint; clang errors.
// loomcc-ref-diverges: tcc [tcc-implicit-int] 816-tcc accepts implicit int
int neg(x) { return -x; } // loomcc-diagnostic
