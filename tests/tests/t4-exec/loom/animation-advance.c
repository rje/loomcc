// loomcc-do: run
// loomcc-int: agnostic
// Frame timers per slot: a u16 elapsed counter against per-frame durations
// from a ROM clip; looping and one-shot clips.
#include "loomcc-test.h"
typedef struct { u8 tile; u8 duration; } Frame;
static const Frame walk[4] = { { 1, 4 }, { 2, 4 }, { 3, 6 }, { 2, 4 } };
static const Frame land[2] = { { 9, 3 }, { 10, 5 } };
#define SLOTS 3
static const Frame *clip[SLOTS];
static u8 count[SLOTS], index_[SLOTS], looping[SLOTS], done[SLOTS];
static u16 elapsed[SLOTS];
static void tick(void) {
  u8 s;
  for (s = 0; s < SLOTS; s++) {
    if (done[s] || !clip[s]) continue;
    elapsed[s]++;
    if (elapsed[s] >= clip[s][index_[s]].duration) {
      elapsed[s] = 0;
      if (index_[s] + 1 < count[s]) index_[s]++;
      else if (looping[s]) index_[s] = 0;
      else done[s] = 1;
    }
  }
}
int main(void) {
  u16 t;
  clip[0] = walk; count[0] = 4; looping[0] = 1;
  clip[1] = land; count[1] = 2; looping[1] = 0;
  for (t = 0; t < 21; t++) tick();
  CHECK(index_[0] == 0 && elapsed[0] == 3);    /* 4+4+6+4 = 18, then 3 more */
  CHECK(done[1] == 1 && index_[1] == 1);
  CHECK(clip[2] == 0 && elapsed[2] == 0);
  return 0;
}
