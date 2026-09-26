// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u32 v = 0x89abcdefu;
  CHECK((u8)(v >> 8) == 0xcd);
  CHECK((u8)(v >> 16) == 0xab);
  CHECK((u16)(v >> 8) == 0xabcd);
  CHECK((i8)(v >> 24) == (i8)0x89);
  CHECK((i16)(v >> 16) == (i16)0x89ab);
  return 0;
}
