// loomcc-do: run
// loomcc-ref-diverges: tcc-rom [tcc-for-empty-cond] 816-tcc compiles for (;; step) with no condition into a jump to itself (the body never runs)
// loomcc-int: agnostic
// A tiny lexer: counts numbers and words in a string.
#include "loomcc-test.h"
enum St { SPACE, NUM, WORD };
int main(void) {
  static const char text[] = "abc 12 x9 7 hello 42z  0";
  const char *p = text;
  enum St st = SPACE;
  int nums = 0, words = 0;
  for (;; p++) {
    char c = *p;
    int digit = c >= '0' && c <= '9';
    int alpha = (c >= 'a' && c <= 'z');
    switch (st) {
    case SPACE:
      if (digit) { st = NUM; nums++; } else if (alpha) { st = WORD; words++; }
      break;
    case NUM:
      if (alpha) { st = WORD; nums--; words++; } else if (!digit) st = SPACE;
      break;
    case WORD:
      if (!digit && !alpha) st = SPACE;
      break;
    }
    if (!c) break;
  }
  CHECK(nums == 3);    /* 12, 7, 0 */
  CHECK(words == 4);   /* abc, x9, hello, 42z (a number that turns into a word) */
  return 0;
}
