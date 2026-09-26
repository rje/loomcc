// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-types] 816-tcc accepts this silently
int *p;
int f(void) { return -p != 0; } // loomcc-error
