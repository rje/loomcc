// loomcc-do: run
// loomcc-int: agnostic
// loomcc-note: F34, reduced (C-Reduce, then by hand) from a T7 --shapes --small --no-foreign program
// A function returning a struct by value, taking a struct and a scalar.
// On the ROM the caller stages the scalar (read from a global) in $00, then
// writes the hidden result pointer over $00/$02, then copies $00 into the
// scalar's argument slot: h receives the frame address as k. The `||` in h
// is what changes the frame layout enough for the move order to go wrong.
#include "loomcc-test.h"
typedef struct { i16 d; u16 arr[2]; } E;
u16 a;
i16 b[8];
E f, g;
static E h(E i, u16 k) {
  i16 j;
  E lv = i;
  b[7] = g.d || 0;
  for (j = 0; j < 5; j++) lv.arr[k % 2u] = (u16)(lv.arr[1] + k);
  return lv;
}
int main(void) {
  f.arr[0] = 10; f.arr[1] = 20;
  f = h(f, a);                       /* k = 0: arr[0] = arr[1] */
  CHECK(f.arr[0] == 20 && f.arr[1] == 20 && b[7] == 0);
  f.arr[0] = 10; f.arr[1] = 20; a = 3;
  f = h(f, a);                       /* k = 3: arr[1] += 3, five times */
  CHECK(f.arr[0] == 10 && f.arr[1] == 35);
  g.d = -2; a = 0;
  f = h(f, a);
  CHECK(f.arr[0] == 35 && b[7] == 1);
  return 0;
}
