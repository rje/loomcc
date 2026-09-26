// loomcc-do: syntax
#include "loomcc-test.h"
typedef char T[7];
STATIC_CHECK(sizeof(T) == 7);
void f(void) {
  double T;
  STATIC_CHECK(sizeof(T) == sizeof(double));
  STATIC_CHECK(sizeof T == sizeof(double));
}
