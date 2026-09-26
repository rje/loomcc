// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-types] 816-tcc accepts this silently
struct S { int a; } s;
int f(void) { return s ? 1 : 0; } // loomcc-error
