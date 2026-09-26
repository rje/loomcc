// loomcc-do: syntax
// loomcc-ref-diverges: clang16 [clang16-ptr16] msp430 pointers are 2 bytes
#include "loomcc-test.h"
#include <stddef.h>
struct V { char c; char *p; };
struct W { char *p; char c; };
struct M { unsigned char *p; };
struct X { int i; char *p; int j; };
STATIC_CHECK(sizeof(struct V) == 8 && offsetof(struct V, p) == 4);
STATIC_CHECK(sizeof(struct W) == 8);
STATIC_CHECK(sizeof(struct M) == 4);
STATIC_CHECK(sizeof(struct X) == 12 && offsetof(struct X, p) == 4 && offsetof(struct X, j) == 8);
