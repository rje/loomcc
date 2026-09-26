// loomcc-do: syntax
#include "loomcc-test.h"
int a[] = { [4] = 1, [1] = 2, 3 };
STATIC_CHECK(sizeof(a) == 5 * sizeof(int));
int b[10] = { [9] = 9, [0] = 0 };
