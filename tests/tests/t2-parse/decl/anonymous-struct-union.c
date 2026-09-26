// loomcc-do: syntax
#include "loomcc-test.h"
struct S { int tag; union { int i; char c; }; struct { int x, y; }; };
STATIC_CHECK(sizeof(struct S) >= 3 * sizeof(int));
int get(struct S *s) { return s->i + s->x + s->y; }
