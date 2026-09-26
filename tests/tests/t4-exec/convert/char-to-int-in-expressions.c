// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 sum(const char *s) { i16 n = 0; while (*s) n += *s++; return n; }
int main(void) {
  static const char hi[] = { (char)0xff, (char)0x80, 1, 0 };
  CHECK(sum(hi) == -1 - 128 + 1);
  CHECK(sum("AB") == 65 + 66);
  return 0;
}
