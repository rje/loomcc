// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-long16] 816-tcc's long is 16 bits and long long 32
#include "loomcc-test.h"
STATIC_CHECK(sizeof(char) == 1);
STATIC_CHECK(sizeof(short) == 2);
STATIC_CHECK(sizeof(int) == 2);
STATIC_CHECK(sizeof(long) == 4);
STATIC_CHECK(sizeof(long long) == 8);
STATIC_CHECK(sizeof(unsigned) == 2);
