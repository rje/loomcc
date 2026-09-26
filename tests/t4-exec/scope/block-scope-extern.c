// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
i16 global_val = 7;
int main(void) {
  extern i16 global_val;
  global_val += 3;
  CHECK(global_val == 10);
  return 0;
}
