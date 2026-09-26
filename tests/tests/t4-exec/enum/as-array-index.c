// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
enum Slot { SLOT_A, SLOT_B, SLOT_C, SLOT_COUNT };
static const u8 cost[SLOT_COUNT] = { 3, 5, 7 };
int main(void) {
  enum Slot s;
  u16 total = 0;
  for (s = SLOT_A; s < SLOT_COUNT; s++) total += cost[s];
  CHECK(total == 15);
  return 0;
}
