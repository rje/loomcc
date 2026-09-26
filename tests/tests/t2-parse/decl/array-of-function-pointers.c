// loomcc-do: syntax
#include "loomcc-test.h"
int h0(int x) { return x; }
int h1(int x) { return -x; }
int (*table[2])(int) = { h0, h1 };
int (*const ctable[])(int) = { h0, h1, h0 };
STATIC_CHECK(sizeof(ctable) / sizeof(ctable[0]) == 3);
