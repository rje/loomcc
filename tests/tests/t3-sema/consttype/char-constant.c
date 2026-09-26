// loomcc-do: syntax
#include "loomcc-test.h"
STATIC_CHECK(sizeof('a') == sizeof(int));
STATIC_CHECK('\377' == -1);
STATIC_CHECK(L'a' == 97);
