// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
enum Dir { NORTH, EAST = 5, SOUTH, WEST = -1 };
static int turn(enum Dir d) {
  switch (d) { case NORTH: return 1; case EAST: return 2; case SOUTH: return 3; case WEST: return 4; }
  return 0;
}
int main(void) {
  enum Dir d = SOUTH;
  CHECK(SOUTH == 6 && WEST == -1);
  CHECK(turn(d) == 3 && turn(WEST) == 4 && turn(NORTH) == 1);
  d = (enum Dir)5;
  CHECK(d == EAST);
  return 0;
}
