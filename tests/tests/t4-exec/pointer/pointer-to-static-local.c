// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 *counter(void) { static i16 c; return &c; }
int main(void) {
  *counter() += 5;
  (*counter())++;
  CHECK(*counter() == 6);
  return 0;
}
