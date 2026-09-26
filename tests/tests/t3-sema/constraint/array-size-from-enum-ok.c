// loomcc-do: syntax
#include "loomcc-test.h"
enum { N = 5, M = N * 2 };
static u8 a[M];
static u8 b[sizeof(a) / N];
STATIC_CHECK(sizeof(a) == 10 && sizeof(b) == 2);
