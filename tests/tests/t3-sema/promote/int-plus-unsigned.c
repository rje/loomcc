// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-fold-host-int] 816-tcc folds unsigned int constant arithmetic in 32 bits (-1 + 0u != 65535u, -1 / 2u == 0x7fffffff)
#include "loomcc-test.h"
STATIC_CHECK(-1 + 0u == 65535u);
STATIC_CHECK((-1 < 1u) == 0);
STATIC_CHECK(-1 > 0u);
STATIC_CHECK(-1 / 2u == 32767u);
STATIC_CHECK(-1 % 7u == 65535u % 7u);
