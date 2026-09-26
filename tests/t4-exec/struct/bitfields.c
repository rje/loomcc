// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct B { unsigned a : 3; unsigned b : 5; unsigned c : 8; };
int main(void) {
  struct B x;
  x.a = 7; x.b = 31; x.c = 255;
  CHECK(x.a == 7 && x.b == 31 && x.c == 255);
  x.a = 8;                 /* truncates to 0 */
  CHECK(x.a == 0 && x.b == 31);
  x.b = x.b + 1;
  CHECK(x.b == 0 && x.c == 255);
  x.c -= 1;
  CHECK(x.c == 254 && x.a == 0);
  return 0;
}
