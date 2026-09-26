// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
i16 tent;
i16 tent;
i16 tent2;
i16 tent2 = 7;
int main(void) {
  CHECK(tent == 0 && tent2 == 7);
  tent = 3;
  CHECK(tent == 3);
  return 0;
}
