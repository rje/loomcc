// loomcc-do: syntax
#include "loomcc-test.h"
enum E { A = 1, B = -1 };
STATIC_CHECK(sizeof(A) == sizeof(int));
STATIC_CHECK(B < 0 && A - 2 < 0);
