// loomcc-do: run
// loomcc-int: agnostic
// A const table of const strings in ROM, as Loom's UI text tables.
#include "loomcc-test.h"
static const char *const names[] = { "START", "OPTIONS", "", "QUIT" };
static u8 len(const char *s) { u8 n = 0; while (s[n]) n++; return n; }
int main(void) {
  u8 i, total = 0;
  for (i = 0; i < sizeof names / sizeof names[0]; i++) total = (u8)(total + len(names[i]));
  CHECK(total == 16);
  CHECK(names[1][6] == 'S' && names[2][0] == 0);
  return 0;
}
