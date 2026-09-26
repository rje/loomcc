// loomcc-do: syntax
#include "loomcc-test.h"
int arr[3];
int (*g(int x))[3] { (void)x; return &arr; }
int (*(*pf)(int))[3] = g;
STATIC_CHECK(sizeof((*pf)(1)[0]) == sizeof(int[3]));
