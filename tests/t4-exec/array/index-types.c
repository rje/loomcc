// loomcc-do: run
// loomcc-int: agnostic
// Indexing with every integer type, including negative indices through a pointer.
#include "loomcc-test.h"
static i16 a[300];
int main(void) {
  u8 i8v = 200;
  i8 s8 = -3;
  u16 u16v = 299;
  i32 i32v = 150;
  i16 *mid = &a[150];
  int i;
  for (i = 0; i < 300; i++) a[i] = (i16)i;
  CHECK(a[i8v] == 200);
  CHECK(mid[s8] == 147);
  CHECK(a[u16v] == 299);
  CHECK(a[i32v] == 150);
  CHECK(mid[-150] == 0);
  CHECK(*(mid + s8) == 147);
  return 0;
}
