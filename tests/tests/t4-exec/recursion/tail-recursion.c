// loomcc-do: run
// loomcc-int: agnostic
// Tail calls to itself become loops: depths that would overflow a stack of
// real frames run in constant space.
#include "loomcc-test.h"
static u16 gcd(u16 a, u16 b) { if (b == 0) return a; return gcd(b, (u16)(a % b)); }
static u16 count_down(u16 n, u16 acc) { if (n == 0) return acc; return count_down((u16)(n - 1), (u16)(acc + 1)); }
static u16 steps;
static void spin(u16 n) { if (n == 0) return; steps++; spin((u16)(n - 1)); }
int main(void) {
  CHECK(gcd(1071, 462) == 21);
  CHECK(count_down(20000, 0) == 20000);
  spin(30000);
  CHECK(steps == 30000);
  return 0;
}
