// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: actor-directions.expected-output
// loomcc-asm-sources: actor-directions.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/actor.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: the actors' direction vectors for every direction byte.
#include "../loom-d88b68b/runtime/src/actor.c"
int printf(const char *fmt, ...);
int main(void) {
  loom_u16 d;
  for (d = 0; d < 256; d += 5) {
    loom_s8 x, y;
    loom_actor_direction_vector((loom_u8)d, &x, &y);
    printf("%u:%d,%d%s", d, (int)x, (int)y, (d % 50 == 45) ? "\n" : " ");
  }
  printf("\n");
  return 0;
}
