// loomcc-do: syntax
// long (32 bits) can hold every unsigned int (16 bits): unsigned int converts
// to long and the comparison is signed (6.3.1.8p1).
// loomcc-ref-diverges: tcc [tcc-long16] 816-tcc's long is 16 bits
#include "loomcc-test.h"
STATIC_CHECK(-1L < 1u);
STATIC_CHECK(-1L < 65535u);
STATIC_CHECK(sizeof(1L + 1u) == 4);
STATIC_CHECK(65535u + 1L == 65536L);
