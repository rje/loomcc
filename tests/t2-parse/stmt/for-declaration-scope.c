// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-no-for-decl] 816-tcc rejects declarations in for (C99)
int f(void) { int i = 10; for (int i = 0; i < 3; i++) {} return i; }
