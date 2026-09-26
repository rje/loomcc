// loomcc-do: syntax
// A struct containing a pointer is 4-aligned wherever it is embedded.
// loomcc-ref-diverges: clang16 [clang16-ptr16] msp430 pointers are 2 bytes
#include "loomcc-test.h"
#include <stddef.h>
struct A { unsigned char a; unsigned char *p; unsigned char b; };
struct K { char c; struct A a; };
STATIC_CHECK(sizeof(struct K) == 16);
STATIC_CHECK(offsetof(struct K, a) == 4);
