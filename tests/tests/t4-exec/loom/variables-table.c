// loomcc-do: run
// loomcc-int: agnostic
// A game-variable table: u8 flags packed in bytes and s16 counters, addressed
// by ids from generated ROM data.
#include "loomcc-test.h"
enum { FLAG_COUNT = 20, COUNTER_COUNT = 5 };
static u8 flag_bits[(FLAG_COUNT + 7) / 8];
static i16 counters[COUNTER_COUNT];
static const i16 counter_min[COUNTER_COUNT] = { 0, -100, 0, 0, -5 };
static const i16 counter_max[COUNTER_COUNT] = { 99, 100, 9999, 3, 5 };
static void flag_set(u8 id, u8 on) { u8 m = (u8)(1u << (id & 7)); if (on) flag_bits[id >> 3] |= m; else flag_bits[id >> 3] &= (u8)~m; }
static u8 flag_get(u8 id) { return (u8)((flag_bits[id >> 3] >> (id & 7)) & 1); }
static void counter_add(u8 id, i16 d) {
  i16 v = (i16)(counters[id] + d);
  if (v < counter_min[id]) v = counter_min[id];
  if (v > counter_max[id]) v = counter_max[id];
  counters[id] = v;
}
int main(void) {
  u8 i;
  for (i = 0; i < FLAG_COUNT; i += 3) flag_set(i, 1);
  flag_set(9, 0);
  CHECK(flag_get(0) && flag_get(3) && !flag_get(9) && flag_get(18) && !flag_get(19) && !flag_get(1));
  CHECK(flag_bits[0] == 0x49 && flag_bits[1] == 0x90 && flag_bits[2] == 0x04);
  counter_add(1, -150); CHECK(counters[1] == -100);
  counter_add(1, 30); CHECK(counters[1] == -70);
  counter_add(3, 7); CHECK(counters[3] == 3);
  counter_add(2, 9000); counter_add(2, 9000); CHECK(counters[2] == 9999);
  counter_add(4, -1); CHECK(counters[4] == -1);
  return 0;
}
