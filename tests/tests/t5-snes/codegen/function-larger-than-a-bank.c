// loomcc-do: run
// loomcc-int: agnostic
// loomcc-ref: host host16
// loomcc-note: F28. A function whose code is larger than a 32 KiB LoROM bank
// loomcc-note: is split across banks: branches between the parts become jml,
// loomcc-note: jump-table entries go through trampolines, and a loop around the
// loomcc-note: whole body branches back across every part. The unrolled
// loomcc-note: function must agree with the same computation as a loop.
#include "loomcc-test.h"
static const u16 v[8] = { 3, 141, 59, 26, 5358, 979, 323, 846 };
static u16 sw;
#define STEP(i) acc = (u16)(1u * acc * 31u + v[(i) & 7] + (u16)(i));
#define SWITCH(i) switch (acc & 7u) { case 0: sw += 1; break; case 1: sw += 3; break; case 2: sw ^= 5; break; \
  case 3: sw = (u16)(sw + (i)); break; case 4: sw -= 7; break; case 5: sw = (u16)(sw * 3u); break; \
  case 6: sw ^= (u16)(i); break; default: sw += 11; break; }
#define S2(b) STEP(b) STEP((b) + 1)
#define S8(b) S2(b) S2((b) + 2) S2((b) + 4) S2((b) + 6)
#define S32(b) S8(b) S8((b) + 8) S8((b) + 16) S8((b) + 24)
#define S128(b) S32(b) S32((b) + 32) S32((b) + 64) S32((b) + 96) SWITCH(b)
#define S512(b) S128(b) S128((b) + 128) S128((b) + 256) S128((b) + 384)
static u16 unrolled(u16 seed) {
  u16 acc = seed;
  u8 k;
  for (k = 0; k < 2; k++) {
    S512(0) S512(512) S512(1024) S512(1536)
  }
  return acc;
}
static u16 looped(u16 seed) {
  u16 acc = seed, i, b;
  u8 k;
  for (k = 0; k < 2; k++)
    for (b = 0; b < 2048; b += 128) {
      for (i = b; i < b + 128; i++) acc = (u16)(1u * acc * 31u + v[i & 7] + i);
      switch (acc & 7u) { case 0: sw += 1; break; case 1: sw += 3; break; case 2: sw ^= 5; break;
        case 3: sw = (u16)(sw + b); break; case 4: sw -= 7; break; case 5: sw = (u16)(sw * 3u); break;
        case 6: sw ^= b; break; default: sw += 11; break; }
    }
  return acc;
}
int main(void) {
  u16 a, b, sa, sb;
  sw = 0; a = unrolled(17); sa = sw;
  sw = 0; b = looped(17); sb = sw;
  CHECK(a == b);
  CHECK(sa == sb);
  return 0;
}
