// loomcc-do: syntax
// (T)-1 is a cast when T is a type, a subtraction when T is an object.
#include "loomcc-test.h"
typedef unsigned char T;
STATIC_CHECK((T)-1 == 255);
void f(void) {
  int T = 5;
  int r = (T)-1;
  (void)r;
}
