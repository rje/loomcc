// loomcc-do: syntax
// loomcc-ref-diverges: clang16 [clang16-ptr16] msp430 pointers are 2 bytes
#include "loomcc-test.h"
#include <stddef.h>
struct G { char c; void (*fp)(void); char d; };
STATIC_CHECK(sizeof(struct G) == 12);
STATIC_CHECK(offsetof(struct G, fp) == 4);
STATIC_CHECK(offsetof(struct G, d) == 8);
