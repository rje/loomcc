// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct S { u8 a; u8 b[3]; };
int main(void) {
  struct S arr[5];
  CHECK(sizeof(struct S) == 4);
  CHECK(sizeof arr == 20);
  CHECK(sizeof arr / sizeof arr[0] == 5);
  CHECK(sizeof(u8) == 1 && sizeof(i16) == 2 && sizeof(i32) == 4);
  return 0;
}
