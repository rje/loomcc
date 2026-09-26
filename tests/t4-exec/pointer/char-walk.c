// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static void rev(char *s) { char *e = s; char t; while (*e) e++; while (s < --e) { t = *s; *s++ = *e; *e = t; } }
int main(void) {
  char s[] = "loomcc";
  rev(s);
  CHECK(s[0] == 'c' && s[1] == 'c' && s[2] == 'm' && s[5] == 'l' && s[6] == 0);
  return 0;
}
