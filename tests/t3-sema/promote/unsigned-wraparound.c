// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-fold-host-int] 816-tcc folds unsigned int constant arithmetic in 32 bits (-1 + 0u != 65535u, -1 / 2u == 0x7fffffff)
#include "loomcc-test.h"
STATIC_CHECK(65535u + 1u == 0u);
STATIC_CHECK(0u - 1u == 65535u);
STATIC_CHECK(300u * 300u == 24464u);          /* 90000 mod 65536 */
STATIC_CHECK(32768u * 2u == 0u);
STATIC_CHECK((unsigned)-32768 == 32768u);
STATIC_CHECK(~0u == 65535u);
STATIC_CHECK(-(1u) == 65535u);
