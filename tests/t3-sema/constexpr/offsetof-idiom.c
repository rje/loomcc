// loomcc-do: syntax
// loomcc-note: `(unsigned)&((T *)0)->m` is not an integer constant expression
// loomcc-note: in strict C (6.6p6), but gcc, clang and 816-tcc fold it, it is
// loomcc-note: how devkitsnes's offsetof is written, and Loom's runtime uses it
// loomcc-note: in LOOM_STATIC_ASSERTs (runtime/src/actor.c). loomcc must fold it.
#include "loomcc-test.h"
struct Snap { u8 flags; u16 pad_count; u8 *pads; };
STATIC_CHECK((unsigned int)(&((struct Snap *)0)->pad_count) == 2u);
STATIC_CHECK((unsigned int)(&((struct Snap *)0)->pads) == 4u);
STATIC_CHECK((unsigned long)&((struct Snap *)0)->flags == 0);
int arr[(unsigned int)&((struct Snap *)0)->pads];
STATIC_CHECK(sizeof(arr) == 4 * sizeof(int));
