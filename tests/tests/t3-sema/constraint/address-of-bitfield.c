// loomcc-do: syntax
struct S { unsigned b : 3; };
void f(struct S *s) { unsigned *p = &s->b; (void)p; } // loomcc-error
