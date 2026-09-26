// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u8 data[8] = { 250, 251, 252, 253, 254, 255, 1, 2 };
int main(void) {
  u16 sum = 0;
  u8 wrapped = 0;
  int i;
  for (i = 0; i < 8; i++) {
    sum += data[i];
    wrapped += data[i];
  }
  CHECK(sum == 1518);
  CHECK(wrapped == (u8)1518);
  return 0;
}
