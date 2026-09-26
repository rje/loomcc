// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-no-generic] 816-tcc has no _Generic
#include "loomcc-test.h"
#define KIND(x) _Generic((x), char: 1, int: 2, long: 3, default: 0)
STATIC_CHECK(KIND('a') == 2);
STATIC_CHECK(KIND((char)1) == 1);
STATIC_CHECK(KIND(1L) == 3);
STATIC_CHECK(KIND(1.0) == 0);
