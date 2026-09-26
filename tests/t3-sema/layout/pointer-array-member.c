// loomcc-do: syntax
// loomcc-ref-diverges: clang16 [clang16-ptr16] msp430 pointers are 2 bytes
#include "loomcc-test.h"
#include <stddef.h>
struct N { char a; char *p[2]; };
STATIC_CHECK(sizeof(struct N) == 12);
STATIC_CHECK(offsetof(struct N, p) == 4);
