// loomcc-do: run
// loomcc-int: agnostic
// A save block: a struct serialised byte by byte through a u8 pointer with a
// 16-bit additive-and-xor checksum, verified and corrupted.
#include "loomcc-test.h"
typedef struct { u8 version; u8 level; i16 score; u16 flags; u8 name[6]; } Save;
static u16 checksum(const u8 *p, u16 n) {
  u16 a = 0x1234, b = 0;
  while (n--) { a = (u16)(a + *p); b = (u16)(b ^ (u16)(a << 1)); p++; }
  return (u16)(a ^ b);
}
int main(void) {
  Save s;
  u16 c1, c2;
  u8 i;
  s.version = 3; s.level = 12; s.score = -1234; s.flags = 0xa5a5;
  for (i = 0; i < 6; i++) s.name[i] = (u8)('A' + i);
  c1 = checksum((const u8 *)&s, sizeof s);
  c2 = checksum((const u8 *)&s, sizeof s);
  CHECK(c1 == c2);
  ((u8 *)&s)[5] ^= 1;
  CHECK(checksum((const u8 *)&s, sizeof s) != c1);
  ((u8 *)&s)[5] ^= 1;
  CHECK(checksum((const u8 *)&s, sizeof s) == c1);
  CHECK(sizeof s == 12);
  return 0;
}
