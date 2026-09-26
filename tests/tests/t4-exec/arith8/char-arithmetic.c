// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static char upper(char c) { return (char)(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c); }
static u8 digit(char c) { return (u8)(c - '0'); }
int main(void) {
  CHECK(upper('q') == 'Q' && upper('Q') == 'Q' && upper('1') == '1');
  CHECK(digit('7') == 7 && (char)('0' + 3) == '3');
  CHECK('z' - 'a' == 25);
  return 0;
}
