// loomcc-do: syntax
#include "loomcc-test.h"
#include <stddef.h>
enum En { E1, E2 = 300 };
struct Y { char c; enum En e; };
STATIC_CHECK(sizeof(enum En) == 2);
STATIC_CHECK(sizeof(struct Y) == 4 && offsetof(struct Y, e) == 2);
