// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  int i, n;
  n = 0; for (i = 0; i < 10; i++) n += i; CHECK(n == 45);
  n = 0; i = 10; while (i--) n++; CHECK(n == 10 && i == -1);
  n = 0; i = 0; do n++; while (++i < 5); CHECK(n == 5);
  n = 0; for (i = 0; i < 100; i++) { if (i == 7) break; n++; } CHECK(n == 7);
  n = 0; for (i = 0; i < 10; i++) { if (i & 1) continue; n++; } CHECK(n == 5);
  n = 0; i = 0; while (1) { if (++i > 3) break; n += i; } CHECK(n == 6);
  n = 0; for (i = 10; i > 0; i -= 3) n++; CHECK(n == 4);
  return 0;
}
