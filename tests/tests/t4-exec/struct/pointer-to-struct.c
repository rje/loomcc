// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct Node { i16 v; struct Node *next; };
int main(void) {
  struct Node n3 = { 3, 0 }, n2 = { 2, &n3 }, n1 = { 1, &n2 };
  struct Node *p;
  i16 sum = 0;
  for (p = &n1; p; p = p->next) sum += p->v;
  CHECK(sum == 6);
  n1.next->next->v = 30;
  CHECK(n3.v == 30);
  return 0;
}
