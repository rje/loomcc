// loomcc-do: run
// loomcc-int: agnostic
// loomcc-extra-sources: aux/static-other.c
// Two units each with a static of the same name: no clash.
#include "loomcc-test.h"
static i16 hidden = 1;
i16 other_hidden(void);
int main(void) {
  CHECK(hidden == 1);
  CHECK(other_hidden() == 2);
  return 0;
}
