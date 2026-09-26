// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-fold-host-int] 816-tcc folds unsigned int constant arithmetic in 32 bits (-1 + 0u != 65535u, -1 / 2u == 0x7fffffff)
// With 16-bit int, unsigned short does not fit in int, so it promotes to
// unsigned int (C17 6.3.1.1p2). On a 32-bit-int host both sides are int.
#include "loomcc-test.h"
STATIC_CHECK((unsigned short)1 - (unsigned short)2 > 0);
STATIC_CHECK((unsigned short)0 - 1 == 65535u);
STATIC_CHECK(sizeof((unsigned short)1 + (unsigned short)1) == 2);
STATIC_CHECK((unsigned short)65535 + 1 == 0);
