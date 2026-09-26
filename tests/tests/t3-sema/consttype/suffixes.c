// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-long16] 816-tcc's long is 16 bits
#include "loomcc-test.h"
STATIC_CHECK(sizeof(1u) == 2);
STATIC_CHECK(sizeof(1l) == 4);
STATIC_CHECK(sizeof(1ul) == 4);
STATIC_CHECK(sizeof(65536u) == 4);
STATIC_CHECK(sizeof(1ll) == 8);
STATIC_CHECK(sizeof(40000u) == 2);
