// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-register] 816-tcc allows & on a register variable
void f(void) { register int r = 1; int *p = &r; (void)p; } // loomcc-error
