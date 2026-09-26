// loomcc-do: syntax
#include "loomcc-test.h"
STATIC_CHECK('a' == 97);
STATIC_CHECK('\n' == 10);
STATIC_CHECK('\x41' == 65);
STATIC_CHECK('\101' == 65);
STATIC_CHECK('\0' == 0);
STATIC_CHECK(sizeof('a') == sizeof(int));
