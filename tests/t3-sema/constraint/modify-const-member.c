// loomcc-do: syntax
struct S { int a; };
void f(const struct S *s) { s->a = 1; } // loomcc-error
