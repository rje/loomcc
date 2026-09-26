// loomcc-do: syntax
// Decimal constants: int, long, long long (6.4.4.1p5), at 16/32/64 bits.
// loomcc-ref-diverges: tcc [tcc-long16] 816-tcc's long is 16 bits
#include "loomcc-test.h"
STATIC_CHECK(sizeof(32767) == 2);
STATIC_CHECK(sizeof(32768) == 4);
STATIC_CHECK(sizeof(65535) == 4);
STATIC_CHECK(sizeof(2147483647) == 4);
STATIC_CHECK(sizeof(2147483648) == 8);
STATIC_CHECK(32768 > 0);
