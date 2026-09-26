// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static const u16 keys[] = { 1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, 144, 169, 196, 225, 256 };
static int find(u16 k) {
  int lo = 0, hi = 15;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (keys[mid] == k) return mid;
    if (keys[mid] < k) lo = mid + 1; else hi = mid - 1;
  }
  return -1;
}
int main(void) {
  CHECK(find(1) == 0 && find(256) == 15 && find(81) == 8);
  CHECK(find(0) == -1 && find(2) == -1 && find(300) == -1);
  return 0;
}
