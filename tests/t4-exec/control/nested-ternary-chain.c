// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u8 grade(i16 s) { return s >= 90 ? 'A' : s >= 80 ? 'B' : s >= 70 ? 'C' : s >= 0 ? 'D' : '?'; }
int main(void) {
  CHECK(grade(95) == 'A' && grade(80) == 'B' && grade(79) == 'C' && grade(0) == 'D' && grade(-1) == '?');
  return 0;
}
