// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static char s1[] = "abc";
static char s2[8] = "xy";
static const char *s3 = "ptr";
static const char *table[] = { "zero", "one", "two" };
int main(void) {
  CHECK(sizeof(s1) == 4 && s1[2] == 'c' && s1[3] == 0);
  CHECK(s2[1] == 'y' && s2[2] == 0 && s2[7] == 0);
  CHECK(s3[2] == 'r');
  CHECK(table[2][1] == 'w' && table[1][3] == 0);
  return 0;
}
