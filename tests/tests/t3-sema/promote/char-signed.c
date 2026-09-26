// loomcc-do: syntax
// loomcc-note: Plain char is signed (implementation-defined; 816-tcc and loomcc agree).
#include "loomcc-test.h"
STATIC_CHECK((char)0xff == -1);
STATIC_CHECK('\xff' == -1);
STATIC_CHECK((char)128 < 0);
STATIC_CHECK((signed char)-1 == -1);
STATIC_CHECK((unsigned char)-1 == 255);
