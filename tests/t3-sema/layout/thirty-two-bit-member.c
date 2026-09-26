// loomcc-do: syntax
// loomcc-note: Design question Q1 (docs/FINDINGS.md): 816-tcc aligns its
// loomcc-note: 32-bit integer (long long) to 4 inside structs; loomcc aligns
// loomcc-note: its 32-bit long to 2. A shared `i32` typedef would lay out
// loomcc-note: differently in the two compilers.
// loomcc-xfail: design question Q1 (32-bit member alignment)
// loomcc-ref-diverges: clang16 [clang16-long-align] msp430 aligns long to 2
#include "loomcc-test.h"
#include <stddef.h>
struct E32 { char c; i32 l; };
STATIC_CHECK(sizeof(struct E32) == 8);
STATIC_CHECK(offsetof(struct E32, l) == 4);
