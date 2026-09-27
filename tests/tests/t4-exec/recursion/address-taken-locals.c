// loomcc-do: run
// loomcc-int: agnostic
// Every activation of a recursive function has its own locals, including
// ones whose address is taken and passed down the recursion: each level
// writes through its parent's pointer while its own local stays intact.
// (With one static frame per function, all levels shared these locals.)
#include "loomcc-test.h"
static i16 depth_sum(i16 n, i16 *parent) {
  i16 mine = (i16)(n * 10);
  if (parent) *parent += n;          /* the caller's local, not ours */
  if (n > 0) depth_sum((i16)(n - 1), &mine);
  CHECK(mine == (i16)(n * 10 + (n > 0 ? n - 1 : 0)));
  return mine;
}
static void fill(u8 *buf, u8 n) {
  u8 local[4];
  u8 i;
  for (i = 0; i < 4; i++) local[i] = (u8)(n + i);
  if (n > 0) fill(local, (u8)(n - 1));
  for (i = 0; i < 4; i++) CHECK(local[i] == (u8)(n + i + (n > 0 ? 1 : 0)));
  for (i = 0; i < 4; i++) buf[i] = (u8)(buf[i] + 1);
}
int main(void) {
  i16 top = 0;
  u8 b[4] = { 1, 2, 3, 4 };
  CHECK(depth_sum(6, &top) == 65);
  CHECK(top == 6);
  fill(b, 5);
  CHECK(b[0] == 2 && b[3] == 5);
  return 0;
}
