// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: scene-triggers.expected-output
// loomcc-asm-sources: ../loom-d88b68b/runtime/backends/pvsneslib/src/scene.asm scene-triggers.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/scene.c and runtime/backends/pvsneslib/src/scene.asm (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: the scene trigger scan's box test, which the PVSnesLib target
// does in Loom's scene.asm: C built by loomcc calls Loom's assembly with
// LoomSceneBox and LoomSceneTrigger arrays laid out by loomcc (the asm reads
// them at fixed offsets with a 24-byte stride), compared with the 816-tcc
// build.
#include "../loom-d88b68b/runtime/src/scene.c"
int printf(const char *fmt, ...);
static LoomSceneTrigger triggers[16];
int main(void) {
  LoomSceneBox box;
  loom_u8 i, j;
  for (i = 0; i < 16; i++) {
    triggers[i].x = (loom_s16)(i * 37 - 200);
    triggers[i].y = (loom_s16)((i & 3) * 50 - 60);
    triggers[i].width = (loom_u16)(16 + (i & 7) * 8);
    triggers[i].height = (loom_u16)(8 + (i >> 2) * 12);
  }
  printf("stride %u\n", (unsigned)sizeof(LoomSceneTrigger));
  for (i = 0; i < 24; i++) {
    box.left = (loom_s16)(i * 23 - 230);
    box.top = (loom_s16)((i % 5) * 31 - 80);
    box.right = (loom_s16)(box.left + 12 + (i & 3) * 10);
    box.bottom = (loom_s16)(box.top + 24);
    printf("%d,%d:", box.left, box.top);
    for (j = 0; j <= 16; j += 4) printf(" %x", loom_pvs_scene_intersect_mask(&box, triggers, j));
    printf("\n");
  }
  return 0;
}
