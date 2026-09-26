// loomcc-do: syntax
#include "loomcc-test.h"
struct P { int x, y, z; };
struct P p = { .y = 2, .x = 1 };
struct P q = { .x = 1, .z = 3 };
struct P r = { .y = 5, 6 };      /* 6 initialises z */
STATIC_CHECK(sizeof(struct P) == 3 * sizeof(int));
