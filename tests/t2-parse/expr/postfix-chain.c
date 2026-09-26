// loomcc-do: syntax
struct S { int a[3]; struct S *next; int (*fn)(int); };
int f(struct S *s) { return s->next->a[1] + (*s).next->fn(2) + s->fn(s->a[0])++ ; }
// loomcc-note: `s->fn(...)++` increments an rvalue: a constraint violation.
// loomcc-error@-2
