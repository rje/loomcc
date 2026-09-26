// loomcc-do: syntax
#include "loomcc-test.h"
STATIC_CHECK(sizeof("") == 1 && sizeof("\n") == 2 && sizeof("\0\0") == 3 && sizeof("\x41" "B") == 3);
STATIC_CHECK(sizeof("a" "b" "c") == 4);
STATIC_CHECK('\\' == 92 && '\'' == 39 && '\"' == 34 && '\?' == 63 && '\a' == 7 && '\t' == 9);
