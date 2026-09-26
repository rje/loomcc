// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  int a = 0, b = 0;
  if (a++ && b++) CHECK(0);
  CHECK(a == 1 && b == 0);
  if (a++ || b++) {} else CHECK(0);
  CHECK(a == 2 && b == 0);
  if (!(--a) || b++) CHECK(0);      /* !1 is 0, so b++ runs and yields 0 */
  CHECK(a == 1 && b == 1);
  return 0;
}
