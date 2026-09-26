// loomcc-do: syntax
#include "loomcc-test.h"
char s[] = "ab" "cd" "" "e";
STATIC_CHECK(sizeof(s) == 6);
STATIC_CHECK(sizeof("a" "b") == 3);
