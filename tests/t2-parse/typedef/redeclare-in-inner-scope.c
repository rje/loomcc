// loomcc-do: syntax
#include "loomcc-test.h"
typedef char T;
void f(void) {
  int T = 3;               /* T is an object here */
  int x = T * 2;           /* so this is a multiplication */
  (void)x;
  STATIC_CHECK(sizeof(T) == sizeof(int));
}
STATIC_CHECK(sizeof(T) == 1);
