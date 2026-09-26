// loomcc-do: syntax
struct S { int a; };
int f(struct S s) { return s.b; } // loomcc-error
