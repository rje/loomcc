// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 x = 1;
static i16 get_x(void) { return x; }
int main(void) {
  i16 x = 2;
  CHECK(x == 2 && get_x() == 1);
  {
    i16 x = 3;
    CHECK(x == 3);
    { extern i16 get_x_again(void); CHECK(get_x_again() == 1); }
  }
  CHECK(x == 2);
  return 0;
}
i16 get_x_again(void) { return x; }
