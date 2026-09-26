// loomcc-do: syntax
#include "loomcc-test.h"
STATIC_CHECK(1 + 2 * 3 == 7);
STATIC_CHECK((1 << 2 + 1) == 8);
STATIC_CHECK((1 | 2 ^ 3 & 4) == 3);
STATIC_CHECK((1 < 2 == 1) == 1);
STATIC_CHECK((0 && 1 || 1) == 1);
STATIC_CHECK((1 ? 2 : 0 ? 3 : 4) == 2);
STATIC_CHECK(-2 * -3 == 6);
STATIC_CHECK(!0 + !0 == 2);
STATIC_CHECK(~0 == -1);
STATIC_CHECK(10 - 4 - 3 == 3);
STATIC_CHECK(100 / 10 / 5 == 2);
STATIC_CHECK(7 % 4 * 2 == 6);
