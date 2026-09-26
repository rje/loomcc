// loomcc-do: syntax
struct S { int a; };
int f(struct S x, struct S y) { return x == y; } // loomcc-error
