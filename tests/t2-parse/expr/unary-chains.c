// loomcc-do: syntax
#include "loomcc-test.h"
STATIC_CHECK(- -1 == 1 && !!3 == 1 && ~~5 == 5 && -~0 == 1 && ~-1 == 0 && !-0 == 1);
STATIC_CHECK(+ + +1 == 1 && - + - 2 == 2);
