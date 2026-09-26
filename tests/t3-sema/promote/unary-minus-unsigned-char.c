// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-fold-host-int] 816-tcc folds unsigned int constant arithmetic in 32 bits (-1 + 0u != 65535u, -1 / 2u == 0x7fffffff)
#include "loomcc-test.h"
STATIC_CHECK(-(unsigned char)1 == -1);
STATIC_CHECK(-(unsigned short)1 == 65535u);
STATIC_CHECK(!(unsigned short)0 == 1);
