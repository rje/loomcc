// loomcc-do: syntax
// 6.8.6.4p1 (constraint)
// loomcc-ref-diverges: tcc [tcc-lax-return] 816-tcc accepts it
int f(void) { return; } // loomcc-diagnostic
