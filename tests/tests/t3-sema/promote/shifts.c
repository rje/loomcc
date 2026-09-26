// loomcc-do: syntax
#include "loomcc-test.h"
STATIC_CHECK((1u << 15) == 32768u);
STATIC_CHECK(((unsigned char)0x40 << 8) == 0x4000);
STATIC_CHECK(sizeof((unsigned char)1 << 1) == sizeof(int));
STATIC_CHECK(sizeof(1 << 1L) == sizeof(int));      /* the result has the left operand's promoted type */
STATIC_CHECK((0x8000u >> 15) == 1);
STATIC_CHECK((-2 >> 1) == -1);                     /* implementation-defined: arithmetic */
