// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static const u8 grid[4][5] = { { 1, 2, 3, 4, 5 }, { 6, 7, 8, 9, 10 }, { 11, 42, 13, 14, 15 }, { 16, 17, 18, 19, 20 } };
int main(void) {
  u8 r, c, fr = 99, fc = 99;
  for (r = 0; r < 4; r++)
    for (c = 0; c < 5; c++)
      if (grid[r][c] == 42) { fr = r; fc = c; goto found; }
found:
  CHECK(fr == 2 && fc == 1);
  return 0;
}
