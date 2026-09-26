// loomcc-do: run
// loomcc-int: agnostic
// loomcc-extra-sources: aux/extern-def.c
#include "loomcc-test.h"
extern i16 shared_value;
extern i16 shared_array[3];
i16 get_twice(void);
int main(void) {
  CHECK(shared_value == 42);
  CHECK(shared_array[2] == -3);
  shared_value = 5;
  CHECK(get_twice() == 10);
  return 0;
}
