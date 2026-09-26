// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-array-decay-sizeof] 816-tcc gives sizeof(a + 0) the array's size, not a pointer's
#include "loomcc-test.h"
static i16 a[10];
STATIC_CHECK(sizeof(a) == 20);
STATIC_CHECK(sizeof(a + 0) == sizeof(i16 *));
STATIC_CHECK(sizeof(&a) == sizeof(i16 (*)[10]));
STATIC_CHECK(sizeof(*&a) == 20);
STATIC_CHECK(sizeof(&a[0]) == sizeof(i16 *));
