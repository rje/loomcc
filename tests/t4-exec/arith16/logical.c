// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int calls;
static int t(void) { calls++; return 1; }
static int f(void) { calls++; return 0; }
int main(void) {
  calls = 0;
  CHECK((f() && t()) == 0);
  CHECK(calls == 1);
  calls = 0;
  CHECK((t() || f()) == 1);
  CHECK(calls == 1);
  calls = 0;
  CHECK((t() && f()) == 0);
  CHECK(calls == 2);
  CHECK(!0 == 1);
  CHECK(!5 == 0);
  CHECK((3 && 4) == 1);
  return 0;
}
