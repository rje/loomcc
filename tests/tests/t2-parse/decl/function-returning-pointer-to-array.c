// loomcc-do: syntax
#include "loomcc-test.h"
int arr[4];
int (*f(void))[4] { return &arr; }
STATIC_CHECK(sizeof(*f()) == 4 * sizeof(int));
