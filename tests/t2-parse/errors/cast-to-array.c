// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc accepts this silently
int a[2];
void f(void) { (int[2])a; } // loomcc-error
