// loomcc-do: syntax
#include "loomcc-test.h"
STATIC_CHECK((unsigned char)1 - (unsigned char)2 < 0);
STATIC_CHECK((unsigned char)255 + 1 == 256);
STATIC_CHECK(sizeof((unsigned char)1 + (unsigned char)1) == sizeof(int));
STATIC_CHECK(~(unsigned char)0 == -1);
