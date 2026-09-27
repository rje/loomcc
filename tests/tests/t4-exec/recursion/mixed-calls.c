// loomcc-do: run
// loomcc-int: agnostic
// A recursive function calling non-recursive helpers (which keep static
// frames and run with D = 0), with 8-bit, 16-bit and 32-bit values
// crossing each kind of call.
#include "loomcc-test.h"
static u8 table[8] = { 3, 1, 4, 1, 5, 9, 2, 6 };
static i32 scale(i32 v, u8 k) { return v * k + table[k & 7]; }
static u16 mix(u16 a, u16 b) { u16 t = (u16)(a ^ b); return (u16)((t << 3) | (t >> 13)); }
static i32 rec(u8 n, i32 acc, u16 h) {
  i32 local = scale(acc, n);
  u16 m = mix(h, n);
  if (n == 0) return local + m;
  return rec((u8)(n - 1), local, m) - (i32)n;
}
static i32 model(u8 n, i32 acc, u16 h) {
  i32 local, r;
  u16 m, t;
  for (;;) {
    local = acc * n + table[n & 7];
    t = (u16)(h ^ n);
    m = (u16)((t << 3) | (t >> 13));
    if (n == 0) return local + m;
    acc = local; h = m; n = (u8)(n - 1);
  }
  (void)r;
}
static i32 model_total(u8 n, i32 acc, u16 h) {
  i32 sub = 0;
  u8 k;
  for (k = 1; k <= n; k++) sub += k;
  return model(n, acc, h) - sub;
}
int main(void) {
  CHECK(rec(6, 7, 0x1234) == model_total(6, 7, 0x1234));
  CHECK(rec(0, -5, 1) == model_total(0, -5, 1));
  return 0;
}
