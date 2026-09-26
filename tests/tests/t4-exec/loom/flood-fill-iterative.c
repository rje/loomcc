// loomcc-do: run
// loomcc-int: agnostic
// An iterative flood fill with an explicit u8 stack over a small board (no
// recursion: Loom's board code avoids it).
#include "loomcc-test.h"
#define W 8
#define H 6
static u8 board[H][W] = {
  { 1, 1, 1, 1, 1, 1, 1, 1 },
  { 1, 0, 0, 1, 0, 0, 0, 1 },
  { 1, 0, 1, 1, 0, 1, 0, 1 },
  { 1, 0, 0, 0, 0, 1, 0, 1 },
  { 1, 1, 1, 1, 1, 1, 0, 1 },
  { 1, 1, 1, 1, 1, 1, 1, 1 },
};
static u8 stack_x[W * H], stack_y[W * H];
static u16 fill(u8 x0, u8 y0, u8 color) {
  u16 sp = 0, filled = 0;
  stack_x[sp] = x0; stack_y[sp] = y0; sp++;
  while (sp) {
    u8 x, y;
    sp--; x = stack_x[sp]; y = stack_y[sp];
    if (board[y][x] != 0) continue;
    board[y][x] = color;
    filled++;
    if (x > 0) { stack_x[sp] = (u8)(x - 1); stack_y[sp] = y; sp++; }
    if (x < W - 1) { stack_x[sp] = (u8)(x + 1); stack_y[sp] = y; sp++; }
    if (y > 0) { stack_x[sp] = x; stack_y[sp] = (u8)(y - 1); sp++; }
    if (y < H - 1) { stack_x[sp] = x; stack_y[sp] = (u8)(y + 1); sp++; }
  }
  return filled;
}
int main(void) {
  CHECK(fill(1, 1, 7) == 14);
  CHECK(board[3][4] == 7 && board[1][6] == 7 && board[4][6] == 7);
  CHECK(fill(1, 1, 8) == 0);
  return 0;
}
