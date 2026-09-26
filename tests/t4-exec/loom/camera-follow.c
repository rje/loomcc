// loomcc-do: run
// loomcc-int: agnostic
// A camera that follows a target with a dead zone and clamps to the level.
#include "loomcc-test.h"
typedef struct { i16 x, y; } V2;
static V2 cam;
static const i16 LEVEL_W = 1024, LEVEL_H = 448, VIEW_W = 256, VIEW_H = 224;
static void follow(V2 t) {
  i16 left = (i16)(cam.x + 96), right = (i16)(cam.x + 160);
  if (t.x < left) cam.x = (i16)(cam.x - (left - t.x));
  else if (t.x > right) cam.x = (i16)(cam.x + (t.x - right));
  cam.y = (i16)(t.y - VIEW_H / 2);
  if (cam.x < 0) cam.x = 0;
  if (cam.x > LEVEL_W - VIEW_W) cam.x = (i16)(LEVEL_W - VIEW_W);
  if (cam.y < 0) cam.y = 0;
  if (cam.y > LEVEL_H - VIEW_H) cam.y = (i16)(LEVEL_H - VIEW_H);
}
int main(void) {
  V2 t;
  t.x = 128; t.y = 100; follow(t); CHECK(cam.x == 0 && cam.y == 0);
  t.x = 300; follow(t); CHECK(cam.x == 140);
  t.x = 250; follow(t); CHECK(cam.x == 140);
  t.x = 200; follow(t); CHECK(cam.x == 104);
  t.x = 2000; t.y = 1000; follow(t); CHECK(cam.x == 768 && cam.y == 224);
  t.x = -50; t.y = -50; follow(t); CHECK(cam.x == 0 && cam.y == 0);
  return 0;
}
