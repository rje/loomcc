// loomcc-do: syntax
// loomcc-note: Pointers are 24-bit far pointers stored in 4 bytes (816-tcc ABI).
// loomcc-ref-diverges: clang16 [clang16-ptr16] msp430 pointers are 2 bytes
#include "loomcc-test.h"
STATIC_CHECK(sizeof(char *) == 4);
STATIC_CHECK(sizeof(void (*)(void)) == 4);
STATIC_CHECK(sizeof(int **) == 4);
