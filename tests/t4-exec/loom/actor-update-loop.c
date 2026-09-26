// loomcc-do: run
// loomcc-int: agnostic
// An actor pass: struct-of-arrays positions, per-kind behaviour through a
// switch, removal by swap-with-last, and a live count in a u8.
#include "loomcc-test.h"
enum Kind { K_NONE, K_WALKER, K_FLYER, K_TIMER };
#define CAP 10
static u8 kind[CAP], timer[CAP], count;
static i16 x[CAP], y[CAP];
static i8 dx[CAP];
static void spawn(u8 k, i16 px, i16 py, i8 d, u8 t) { kind[count] = k; x[count] = px; y[count] = py; dx[count] = d; timer[count] = t; count++; }
static void remove_at(u8 i) { count--; kind[i] = kind[count]; x[i] = x[count]; y[i] = y[count]; dx[i] = dx[count]; timer[i] = timer[count]; }
static void pass(void) {
  u8 i = 0;
  while (i < count) {
    switch ((enum Kind)kind[i]) {
    case K_WALKER: x[i] = (i16)(x[i] + dx[i]); if (x[i] < 0 || x[i] > 255) dx[i] = (i8)-dx[i]; break;
    case K_FLYER: y[i] = (i16)(y[i] - 2); break;
    case K_TIMER: if (--timer[i] == 0) { remove_at(i); continue; } break;
    default: break;
    }
    i++;
  }
}
int main(void) {
  u8 t;
  spawn(K_WALKER, 250, 0, 3, 0);
  spawn(K_TIMER, 0, 0, 0, 3);
  spawn(K_FLYER, 0, 100, 0, 0);
  spawn(K_TIMER, 0, 0, 0, 1);
  for (t = 0; t < 5; t++) pass();
  CHECK(count == 2);
  CHECK(kind[0] == K_WALKER && kind[1] == K_FLYER);
  CHECK(y[1] == 90);
  CHECK(x[0] == 247 && dx[0] == -3);   /* 253, 256 (turns), 253, 250, 247 */
  return 0;
}
