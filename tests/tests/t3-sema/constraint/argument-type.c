// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-types] 816-tcc converts silently
struct S { int a; };
int g(int a);
int f(struct S s) { return g(s); } // loomcc-error
