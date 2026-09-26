// loomcc-do: syntax
#include "loomcc-test.h"
STATIC_CHECK((char)(short)(long)300 == (char)44);
STATIC_CHECK((unsigned char)(signed char)-1 == 255);
STATIC_CHECK(sizeof((char)1 + (char)1) == sizeof(int));
STATIC_CHECK(sizeof((long)(char)1) == sizeof(long));
