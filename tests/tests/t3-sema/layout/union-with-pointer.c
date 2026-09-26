// loomcc-do: syntax
// loomcc-ref-diverges: clang16 [clang16-ptr16] msp430 pointers are 2 bytes
#include "loomcc-test.h"
#include <stddef.h>
union U { char c; short s; unsigned char *p; };
struct L { char c; union U u; };
STATIC_CHECK(sizeof(union U) == 4);
STATIC_CHECK(sizeof(struct L) == 8);
STATIC_CHECK(offsetof(struct L, u) == 4);
