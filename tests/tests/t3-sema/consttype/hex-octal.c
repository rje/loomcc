// loomcc-do: syntax
// Hex and octal constants may be unsigned: int, unsigned int, long, unsigned long, ...
// loomcc-ref-diverges: tcc [tcc-long16] 816-tcc's long is 16 bits
#include "loomcc-test.h"
STATIC_CHECK(sizeof(0x7fff) == 2);
STATIC_CHECK(sizeof(0x8000) == 2 && 0x8000 - 0x8001 > 0);   /* unsigned int */
STATIC_CHECK(sizeof(0xffff) == 2 && 0xffff + 1 == 0);
STATIC_CHECK(sizeof(0x10000) == 4);
STATIC_CHECK(sizeof(0x80000000) == 4 && 0x80000000 > 0);    /* unsigned long */
STATIC_CHECK(sizeof(0177777) == 2 && 0177777 + 1 == 0);
