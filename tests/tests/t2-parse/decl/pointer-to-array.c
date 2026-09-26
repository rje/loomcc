// loomcc-do: syntax
#include "loomcc-test.h"
int a[3];
int (*pa)[3] = &a;
STATIC_CHECK(sizeof(*pa) == 3 * sizeof(int));
