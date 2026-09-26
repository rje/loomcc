// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int f(u8 x) {
  switch (x) { case 0: return 1; case 128: return 2; case 255: return 3; default: return 0; }
}
static int g(i8 x) {
  switch (x) { case -128: return 1; case -1: return 2; case 127: return 3; default: return 0; }
}
int main(void) {
  CHECK(f(0) == 1 && f(128) == 2 && f(255) == 3 && f(1) == 0);
  CHECK(g(-128) == 1 && g(-1) == 2 && g(127) == 3 && g(0) == 0);
  return 0;
}
