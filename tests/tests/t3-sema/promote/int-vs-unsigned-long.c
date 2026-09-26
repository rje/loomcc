// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-long16] 816-tcc's long is 16 bits
#include "loomcc-test.h"
STATIC_CHECK((-1 < 1ul) == 0);
STATIC_CHECK(-1 + 0ul == 4294967295ul);
STATIC_CHECK(sizeof(1 + 1ul) == 4);
