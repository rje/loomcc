// loomcc-do: run
// loomcc-int: agnostic
// A const table bigger than a page, read with computed indices.
#include "loomcc-test.h"
#define N 600
static const u16 table[N] = {
#define R10(b) b, b+1, b+2, b+3, b+4, b+5, b+6, b+7, b+8, b+9
#define R100(b) R10(b), R10(b+10), R10(b+20), R10(b+30), R10(b+40), R10(b+50), R10(b+60), R10(b+70), R10(b+80), R10(b+90)
  R100(0), R100(100), R100(200), R100(300), R100(400), R100(500)
};
int main(void) {
  u32 sum = 0;
  int i;
  for (i = 0; i < N; i++) { CHECK(table[i] == i); sum += table[i]; }
  CHECK(sum == 179700);
  return 0;
}
