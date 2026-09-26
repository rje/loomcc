// loomcc-do: syntax
#include "loomcc-test.h"
enum A { A0 = -2, A1, A2 = A1 + 10, A3 };
enum { B0 = sizeof(enum A), B1 = A3 * 2 };
STATIC_CHECK(A1 == -1 && A2 == 9 && A3 == 10 && B1 == 20);
STATIC_CHECK(B0 == sizeof(int));
