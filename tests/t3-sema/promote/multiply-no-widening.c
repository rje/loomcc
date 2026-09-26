// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-fold-host-int] 816-tcc folds unsigned int constant arithmetic in 32 bits (-1 + 0u != 65535u, -1 / 2u == 0x7fffffff)
// A 16x16 multiply stays 16 bits: the product of two unsigned shorts wraps.
#include "loomcc-test.h"
STATIC_CHECK((unsigned short)256 * (unsigned short)256 == 0);
STATIC_CHECK((unsigned short)255 * (unsigned short)257 == 65535u);
STATIC_CHECK((unsigned short)300 * 300u == 24464u);
STATIC_CHECK(300L * 300 == 90000L);
