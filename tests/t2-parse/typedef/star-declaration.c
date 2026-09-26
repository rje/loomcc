// loomcc-do: syntax
// `T * x;` declares x when T is a typedef name.
#include "loomcc-test.h"
typedef long T;
void f(void) {
  T * x;
  x = 0;
  STATIC_CHECK(sizeof(x) == sizeof(long *));
}
