// loomcc-do: run
// loomcc-int: agnostic
// Two functions that call each other, each with an array local and a
// pointer to the other's local passed down: every activation of each has
// its own copy.
#include "loomcc-test.h"
static i16 ping(i16 n, i16 *acc);
static i16 pong(i16 n, i16 *acc) {
  i16 v[3];
  i16 r;
  v[0] = n; v[1] = (i16)(n * 2); v[2] = (i16)(n * 3);
  *acc += v[2];
  r = n > 0 ? ping((i16)(n - 1), &v[1]) : 0;
  CHECK(v[0] == n && v[2] == n * 3);
  return (i16)(r + v[1]);
}
static i16 ping(i16 n, i16 *acc) {
  i16 w = n;
  *acc += 1;
  return n > 0 ? (i16)(pong((i16)(n - 1), &w) + w) : w;
}
static u8 is_even(u16 n);
static u8 is_odd(u16 n) { return n == 0 ? 0 : is_even((u16)(n - 1)); }
static u8 is_even(u16 n) { return n == 0 ? 1 : is_odd((u16)(n - 1)); }
int main(void) {
  i16 acc = 0;
  i16 r = ping(7, &acc);
  CHECK(acc == 1);
  CHECK(r == 79);
  CHECK(is_even(40) && !is_even(33) && is_odd(21));
  return 0;
}
