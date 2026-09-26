// loomcc-do: run
// loomcc-int: agnostic
// Each activation keeps its own locals (compiled-stack frames must be saved).
#include "loomcc-test.h"
static i16 depth_sum(i16 n) {
  i16 local = (i16)(n * 3);
  i16 below;
  if (n == 0) return 0;
  below = depth_sum((i16)(n - 1));
  return (i16)(local + below);
}
int main(void) {
  CHECK(depth_sum(20) == 3 * 210);
  return 0;
}
