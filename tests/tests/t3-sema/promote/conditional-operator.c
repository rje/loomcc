// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-fold-host-int] 816-tcc folds unsigned int constant arithmetic in 32 bits (-1 + 0u != 65535u, -1 / 2u == 0x7fffffff)
#include "loomcc-test.h"
STATIC_CHECK((1 ? -1 : 0u) == 65535u);
STATIC_CHECK(sizeof(1 ? (char)0 : (char)0) == sizeof(int));
STATIC_CHECK(sizeof(1 ? (short)0 : (char)0) == 2);
STATIC_CHECK((0 ? 1u : -1) > 0);
