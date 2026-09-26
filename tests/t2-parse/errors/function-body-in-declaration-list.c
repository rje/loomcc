// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc accepts this silently
int f(void), g(void) { return 0; } // loomcc-error
