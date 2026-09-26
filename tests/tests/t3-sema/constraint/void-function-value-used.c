// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-types] 816-tcc accepts this silently
void g(void);
int f(void) { return g() + 1; } // loomcc-error
