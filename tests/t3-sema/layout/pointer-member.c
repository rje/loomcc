// loomcc-do: syntax
// loomcc-note: Values from 816-tcc (scripts/tcc-layout.py): pointers are 4
// loomcc-note: bytes and 4-aligned inside structs (loomcc PLAN section 1).
// loomcc-ref-diverges: clang16 [clang16-ptr16] msp430 pointers are 2 bytes
#include "loomcc-test.h"
#include <stddef.h>
struct A { unsigned char a; unsigned char *p; unsigned char b; };
STATIC_CHECK(sizeof(struct A) == 12);
STATIC_CHECK(offsetof(struct A, p) == 4);
STATIC_CHECK(offsetof(struct A, b) == 8);
STATIC_CHECK(sizeof(struct A[2]) == 24);
