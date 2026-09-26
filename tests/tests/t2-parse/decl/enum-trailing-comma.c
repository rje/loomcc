// loomcc-do: syntax
#include "loomcc-test.h"
enum E { A, B = 5, C, };
enum { D = C + 1 };
STATIC_CHECK(A == 0 && B == 5 && C == 6 && D == 7);
