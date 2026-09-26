// loomcc-do: syntax
#include "loomcc-test.h"
#include <stddef.h>
struct D { char c; short s; };
struct J { short s; char c; };
struct H { char a[3]; };
struct I { struct H h; char c; };
STATIC_CHECK(sizeof(struct D) == 4 && offsetof(struct D, s) == 2);
STATIC_CHECK(sizeof(struct J) == 4);
STATIC_CHECK(sizeof(struct H) == 3);
STATIC_CHECK(sizeof(struct I) == 4 && offsetof(struct I, c) == 3);
