// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: movement-step.expected-output
// loomcc-asm-sources: movement-step.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/movement.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: movement's subpixel step (the per-tick position update for
// every moving body) over speeds, subpixels and directions, compared with
// the 816-tcc build.
#include "../loom-d88b68b/runtime/src/movement.c"
int printf(const char *fmt, ...);
int main(void) {
  static const loom_s16 starts[] = { 0, -1, 100, -100, 32000 };
  static const loom_u8 subs[] = { 0, 1, 128, 255 };
  static const loom_u16 speeds[] = { 1, 255, 256, 384, 1000, 0x7fff };
  static const loom_s8 dirs[] = { -1, 0, 1 };
  loom_u8 a, b, c, d;
  for (a = 0; a < 5; a++)
    for (c = 0; c < 6; c++) {
      printf("%d/%u:", starts[a], speeds[c]);
      for (b = 0; b < 4; b++)
        for (d = 0; d < 3; d++) {
          loom_s16 next;
          loom_u8 nsub;
          loom_movement_step(starts[a], subs[b], dirs[d], speeds[c], &next, &nsub);
          printf(" %d.%u", next, (unsigned)nsub);
        }
      printf("\n");
    }
  return 0;
}
