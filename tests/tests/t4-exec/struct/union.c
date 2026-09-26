// loomcc-do: run
// loomcc-int: agnostic
// loomcc-note: Reading a different union member than was written reinterprets
// loomcc-note: the bytes (C17 6.5.2.3 footnote); both targets are little-endian.
// loomcc-skip-mode: host
#include "loomcc-test.h"
union U { u16 w; u8 b[2]; };
int main(void) {
  union U u;
  u.w = 0x1234;
  CHECK(u.b[0] == 0x34 && u.b[1] == 0x12);
  u.b[1] = 0xab;
  CHECK(u.w == 0xab34);
  CHECK(sizeof(union U) == 2);
  return 0;
}
