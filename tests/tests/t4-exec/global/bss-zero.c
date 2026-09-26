// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u8 bytes[100];
static i32 words[10];
struct S { i16 a; u8 b[5]; };
static struct S s;
int main(void) {
  int i;
  for (i = 0; i < 100; i++) CHECK(bytes[i] == 0);
  for (i = 0; i < 10; i++) CHECK(words[i] == 0);
  CHECK(s.a == 0 && s.b[4] == 0);
  return 0;
}
