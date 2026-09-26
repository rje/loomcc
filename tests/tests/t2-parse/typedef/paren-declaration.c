// loomcc-do: syntax
// `T(x);` declares x of type T when T is a typedef name.
#include "loomcc-test.h"
typedef char T;
void f(void) {
  T(x);
  x = 1;
  STATIC_CHECK(sizeof(x) == 1);
}
