// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static void zero_row(u8 row[8]) { u8 i; for (i = 0; i < 8; i++) row[i] = 0; }
static void fill(u8 m[][8], u8 rows, u8 v) { u8 r, c; for (r = 0; r < rows; r++) for (c = 0; c < 8; c++) m[r][c] = v; }
int main(void) {
  u8 m[3][8];
  fill(m, 3, 9);
  zero_row(m[1]);
  CHECK(m[0][7] == 9 && m[1][0] == 0 && m[1][7] == 0 && m[2][3] == 9);
  return 0;
}
