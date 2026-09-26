// loomcc-do: syntax
#include "loomcc-test.h"
int *ap[3];
STATIC_CHECK(sizeof(ap) == 3 * sizeof(int *));
STATIC_CHECK(sizeof(ap[0]) == sizeof(int *));
