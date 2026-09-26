// loomcc-do: syntax
#include "loomcc-test.h"
STATIC_CHECK(sizeof(int *) == sizeof(void *));
STATIC_CHECK(sizeof(int[5]) == 5 * sizeof(int));
STATIC_CHECK(sizeof(int (*)[5]) == sizeof(void *));
STATIC_CHECK(sizeof(int *[5]) == 5 * sizeof(int *));
STATIC_CHECK(sizeof(int (*)(void)) == sizeof(void (*)(void)));
STATIC_CHECK(sizeof(int (*[3])(int)) == 3 * sizeof(int (*)(int)));
STATIC_CHECK(sizeof(char (*(*)[2])[3]) == sizeof(void *));
