// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct Grid { u8 w, h; u8 cells[4][5]; };
static struct Grid g;
static void set(struct Grid *p, int r, int c, u8 v) { p->cells[r][c] = v; }
int main(void) {
  u8 (*row)[5];
  set(&g, 3, 4, 99);
  set(&g, 0, 0, 1);
  row = g.cells;
  CHECK(row[3][4] == 99 && (*row)[0] == 1);
  CHECK(&g.cells[1][0] == &g.cells[0][5]);
  return 0;
}
