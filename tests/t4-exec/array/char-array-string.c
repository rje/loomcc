// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int len(const char *s) { int n = 0; while (*s++) n++; return n; }
int main(void) {
  char s[] = "hello";
  const char *t = "world!";
  CHECK(sizeof(s) == 6);
  CHECK(len(s) == 5 && len(t) == 6);
  CHECK(s[4] == 'o' && s[5] == 0);
  s[0] = 'j';
  CHECK(s[0] == 'j' && t[0] == 'w');
  CHECK("abc"[1] == 'b');
  return 0;
}
