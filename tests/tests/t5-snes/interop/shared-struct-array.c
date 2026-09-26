// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-actors.c
// An array of structs defined by the unit, walked and modified by 816-tcc
// through a pointer and an index (the stride must match: 816-tcc layout).
#include "loomcc-test.h"
typedef struct { u8 kind; u8 *name; i16 x; i32 score; } Actor;
Actor unit_actors[5];
static u8 names[5] = { 'a', 'b', 'c', 'd', 'e' };
u16 tcc_actor_size(void);
i32 tcc_total_score(Actor *base, u8 n);
void tcc_move(u8 i, i16 dx);
int main(void) {
  u8 i;
  for (i = 0; i < 5; i++) { unit_actors[i].kind = i; unit_actors[i].name = &names[i]; unit_actors[i].x = (i16)(i * 100); unit_actors[i].score = (i32)i * 70000; }
  CHECK(sizeof(Actor) == tcc_actor_size());
  CHECK(tcc_total_score(unit_actors, 5) == (i32)700000 + 'a' + 'b' + 'c' + 'd' + 'e');
  tcc_move(3, -350);
  CHECK(unit_actors[3].x == -50 && unit_actors[2].x == 200 && unit_actors[4].x == 400);
  return 0;
}
