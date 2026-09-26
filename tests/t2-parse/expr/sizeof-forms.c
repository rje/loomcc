// loomcc-do: syntax
#include "loomcc-test.h"
struct S { char a[5]; };
struct S s;
STATIC_CHECK(sizeof s == 5 && sizeof(s) == 5 && sizeof s.a == 5 && sizeof(struct S) == 5);
STATIC_CHECK(sizeof s.a[0] == 1 && sizeof -s.a[0] == sizeof(int) && sizeof(s.a[0]) + 1 == 2);
STATIC_CHECK(sizeof(int) * 2 == 2 * sizeof(int));
