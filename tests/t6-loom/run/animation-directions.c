// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: animation-directions.expected-output
// loomcc-ref: tcc-rom
// loomcc-source: Loom d88b68b runtime/src/animation.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: direction slot selection for every mode and facing pair,
// compared with the 816-tcc build.
#include "../loom-d88b68b/runtime/src/animation.c"
int printf(const char *fmt, ...);
int main(void) {
  loom_u8 mode, mirror;
  loom_s8 fx, fy;
  for (mode = 0; mode < 6; mode++) {
    printf("mode %u count %u:", (unsigned)mode, (unsigned)loom_animation_direction_count(mode));
    for (fy = -1; fy <= 1; fy++)
      for (fx = -1; fx <= 1; fx++) {
        loom_u8 slot = loom_animation_direction_for(mode, fx, fy, &mirror);
        printf(" %u/%u", (unsigned)slot, (unsigned)mirror);
      }
    printf("\n");
  }
  return 0;
}
