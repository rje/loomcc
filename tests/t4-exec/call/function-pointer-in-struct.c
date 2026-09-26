// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct Hook { u8 id; void (*fn)(i16 *); };
static void inc(i16 *p) { (*p)++; }
static void dbl(i16 *p) { *p *= 2; }
static struct Hook hooks[3] = { { 1, inc }, { 2, dbl }, { 3, inc } };
int main(void) {
  i16 v = 5;
  int i;
  for (i = 0; i < 3; i++) hooks[i].fn(&v);
  CHECK(v == 13);
  return 0;
}
