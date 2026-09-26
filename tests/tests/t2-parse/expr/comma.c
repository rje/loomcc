// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-no-for-decl] 816-tcc rejects declarations in for (C99)
int f(int a) { int b = (a++, a++, a); for (int i = 0, j = 1; i < j; i++, j--) {} return b; }
