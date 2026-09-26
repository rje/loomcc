// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-crash] 816-tcc crashes on sizeof of an assignment expression
#include "loomcc-test.h"
unsigned char uc;
signed char sc;
unsigned short us;
STATIC_CHECK(sizeof(uc += 1) == 1);
STATIC_CHECK(sizeof(us *= 2) == 2);
STATIC_CHECK(sizeof(sc = 1000) == 1);
STATIC_CHECK(sizeof(uc++) == 1);
