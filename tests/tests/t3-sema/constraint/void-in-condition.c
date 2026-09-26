// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-types] 816-tcc accepts this silently
void g(void);
int f(void) { if (g()) return 1; return 0; } // loomcc-error
