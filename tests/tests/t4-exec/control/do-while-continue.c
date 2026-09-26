// loomcc-do: run
// loomcc-int: agnostic
// continue in a do-while jumps to the condition, not the top.
#include "loomcc-test.h"
int main(void) {
  u8 i = 0, body = 0;
  do { i++; if (i & 1) continue; body++; } while (i < 10);
  CHECK(i == 10 && body == 5);
  return 0;
}
