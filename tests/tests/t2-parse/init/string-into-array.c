// loomcc-do: syntax
#include "loomcc-test.h"
char s1[] = "abc";
char s2[4] = "abc";
char s3[3] = "abc";          /* no room for the null: allowed in C */
char s4[] = { "braced" };
STATIC_CHECK(sizeof(s1) == 4 && sizeof(s3) == 3 && sizeof(s4) == 7);
