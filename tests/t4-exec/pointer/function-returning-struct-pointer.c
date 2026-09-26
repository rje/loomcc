// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
typedef struct { u8 id; i16 hp; } Actor;
static Actor actors[4] = { { 1, 10 }, { 2, 20 }, { 3, 30 }, { 4, 40 } };
static Actor *find(u8 id) { u8 i; for (i = 0; i < 4; i++) if (actors[i].id == id) return &actors[i]; return 0; }
int main(void) {
  Actor *a = find(3);
  CHECK(a && a->hp == 30);
  find(2)->hp -= 5;
  CHECK(actors[1].hp == 15);
  CHECK(find(9) == 0);
  return 0;
}
