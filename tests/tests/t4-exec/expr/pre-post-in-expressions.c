// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  i16 arr[4] = { 10, 20, 30, 40 };
  i16 *p = arr;
  i16 v;
  v = *p++;       CHECK(v == 10 && p == &arr[1]);
  v = *++p;       CHECK(v == 30 && p == &arr[2]);
  v = ++*p;       CHECK(v == 31 && arr[2] == 31);
  v = (*p)++;     CHECK(v == 31 && arr[2] == 32);
  v = *p--;       CHECK(v == 32 && p == &arr[1]);
  return 0;
}
