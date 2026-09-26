// loomcc-do: run
// loomcc-int: agnostic
// Word wrap for a text box: count the lines a ROM string needs at a width.
#include "loomcc-test.h"
static u8 lines_needed(const char *s, u8 width) {
  u8 lines = 1, col = 0;
  while (*s) {
    const char *w = s;
    u8 len = 0;
    while (*w && *w != ' ' && *w != '\n') { w++; len++; }
    if (col && col + 1 + len > width) { lines++; col = 0; }
    col = (u8)(col + (col ? 1 : 0) + len);
    s = w;
    if (*s == '\n') { lines++; col = 0; }
    if (*s) s++;
  }
  return lines;
}
int main(void) {
  static const char msg[] = "the quick brown fox jumps over the lazy dog";
  CHECK(lines_needed(msg, 43) == 1);
  CHECK(lines_needed(msg, 20) == 3);
  CHECK(lines_needed(msg, 10) == 5);
  CHECK(lines_needed("a\nb\nc", 10) == 3);
  CHECK(lines_needed("", 10) == 1);
  return 0;
}
