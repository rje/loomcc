// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u8 grid[5][7];
int main(void) {
  int r, c;
  u16 sum = 0;
  for (r = 0; r < 5; r++)
    for (c = 0; c < 7; c++)
      grid[r][c] = (u8)(r * 7 + c);
  for (r = 0; r < 5; r++) sum += grid[r][6];
  CHECK(sum == 6 + 13 + 20 + 27 + 34);
  CHECK(grid[4][6] == 34);
  CHECK(&grid[1][0] - &grid[0][0] == 7);
  CHECK(sizeof(grid[0]) == 7);
  return 0;
}
