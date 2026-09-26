// loomcc-do: syntax
#include "loomcc-test.h"
static const char s1[] = "hello";
static const char s2[10] = "hi";
static const u8 s3[] = { 'a', 'b' };
STATIC_CHECK(sizeof(s1) == 6 && sizeof(s2) == 10 && sizeof(s3) == 2);
