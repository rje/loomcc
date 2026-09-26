// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-tag-scope] 816-tcc rejects redeclaring a struct tag in an inner block
#include "loomcc-test.h"
struct S { char a; };
void f(void) {
  struct S { char a[4]; } inner;
  STATIC_CHECK(sizeof(inner) == 4);
  { struct S; struct S { int x[3]; } innermost; STATIC_CHECK(sizeof(innermost) == 3 * sizeof(int)); }
}
STATIC_CHECK(sizeof(struct S) == 1);
