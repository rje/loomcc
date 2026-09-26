// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 m[3][4];
static i16 rowsum(i16 (*r)[4]) { return (i16)((*r)[0] + (*r)[1] + (*r)[2] + (*r)[3]); }
int main(void) {
  int i, j;
  for (i = 0; i < 3; i++) for (j = 0; j < 4; j++) m[i][j] = (i16)(i * 10 + j);
  CHECK(rowsum(&m[2]) == 20 + 21 + 22 + 23);
  CHECK(rowsum(m + 1) == 10 + 11 + 12 + 13);
  return 0;
}
