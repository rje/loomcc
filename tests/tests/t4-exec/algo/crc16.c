// loomcc-do: run
// loomcc-int: agnostic
// CRC-16/CCITT-FALSE of "123456789" is 0x29B1.
#include "loomcc-test.h"
static u16 crc16(const u8 *d, u16 n) {
  u16 crc = 0xffff;
  int i;
  while (n--) {
    crc ^= (u16)(*d++ << 8);
    for (i = 0; i < 8; i++) crc = (crc & 0x8000) ? (u16)((crc << 1) ^ 0x1021) : (u16)(crc << 1);
  }
  return crc;
}
int main(void) {
  static const u8 msg[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  CHECK(crc16(msg, 9) == 0x29b1);
  return 0;
}
