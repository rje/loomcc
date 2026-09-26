// loomcc-do: syntax
// loomcc-note: 816-tcc aligns its 32-bit integer (long long) to 4 inside
// loomcc-note: structs; loomcc's 32-bit long must match, so a shared `i32`
// loomcc-note: typedef lays out the same in both (was Q1 in docs/FINDINGS.md).
// loomcc-ref-diverges: clang16 [clang16-long-align] msp430 aligns long to 2
#include "loomcc-test.h"
#include <stddef.h>
struct E32 { char c; i32 l; };
STATIC_CHECK(sizeof(struct E32) == 8);
STATIC_CHECK(offsetof(struct E32, l) == 4);
