// loomcc-do: syntax
#include "loomcc-test.h"
int a, *b, c[2], (*d)(void), e = 3, *f = &e;
STATIC_CHECK(sizeof(c) == 2 * sizeof(int));
STATIC_CHECK(sizeof(a) == sizeof(int));
