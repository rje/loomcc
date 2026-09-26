// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct Item { i16 key; u8 id; };
int main(void) {
  struct Item a[7] = { { 5, 0 }, { -2, 1 }, { 9, 2 }, { 5, 3 }, { 0, 4 }, { -2, 5 }, { 100, 6 } };
  int i, j;
  for (i = 1; i < 7; i++) {
    struct Item t = a[i];
    for (j = i - 1; j >= 0 && a[j].key > t.key; j--) a[j + 1] = a[j];
    a[j + 1] = t;
  }
  CHECK(a[0].key == -2 && a[0].id == 1 && a[1].id == 5);   /* stable */
  CHECK(a[3].key == 5 && a[3].id == 0 && a[4].id == 3);
  CHECK(a[6].key == 100);
  return 0;
}
