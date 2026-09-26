// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I%ROOT%/harness/libc
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// Calls into PVSnesLib's libc (compiled by 816-tcc): memcpy, memset, memcmp,
// strlen, strcmp, strcpy, with pointers into WRAM and ROM.
#include <string.h>
#include "loomcc-test.h"
static const char greeting[] = "hello, snes";
static char buf[32];
int main(void) {
  u8 i;
  memset(buf, 'x', sizeof buf);
  CHECK(buf[0] == 'x' && buf[31] == 'x');
  memcpy(buf, greeting, sizeof greeting);
  CHECK(strlen(buf) == 11 && strcmp(buf, greeting) == 0);
  CHECK(memcmp(buf, "hello", 5) == 0 && memcmp(buf, "help", 4) < 0);
  strcpy(buf + 12, "ab");
  CHECK(strcmp(buf + 12, "ab") == 0 && strcmp("aa", buf + 12) < 0);
  for (i = 0; i < 11; i++) CHECK(buf[i] == greeting[i]);
  return 0;
}
