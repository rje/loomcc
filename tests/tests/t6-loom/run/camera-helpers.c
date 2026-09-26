// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: camera-helpers.expected-output
// loomcc-ref: tcc-rom
// loomcc-source: Loom d88b68b runtime/src/camera.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: the camera's pure helpers (facing sign, auto-scroll step with a
// carried 1/256 fraction, approach), over grids of inputs, compared with the
// 816-tcc build. The unit also references body.asm symbols; the harness
// never calls them.
#include "../loom-d88b68b/runtime/src/camera.c"
int printf(const char *fmt, ...);
int main(void) {
  static const loom_s16 speeds[] = { 0, 1, 255, 256, 257, 384, 1000, -1, -255, -256, -257, -384, -1000, 32767, -32767 };
  static const loom_s8 facings[] = { -128, -1, 0, 1, 127 };
  static const loom_s16 goals[] = { -300, -17, -1, 0, 1, 5, 16, 17, 40, 300 };
  loom_u8 i, t;
  for (i = 0; i < sizeof facings; i++) printf("sign %d=%d\n", (int)facings[i], loom_camera_facing_sign(facings[i]));
  for (i = 0; i < sizeof speeds / sizeof speeds[0]; i++) {
    loom_s16 frac = 0, pos = 0;
    for (t = 0; t < 5; t++) pos = (loom_s16)(pos + loom_camera_auto_step(speeds[i], &frac));
    printf("auto %d -> %d frac %d\n", speeds[i], pos, frac);
  }
  for (i = 0; i < sizeof goals / sizeof goals[0]; i++) {
    loom_s16 cam = 0;
    for (t = 0; t < 4; t++) cam = loom_camera_approach(cam, goals[i]);
    printf("approach %d -> %d\n", goals[i], cam);
  }
  return 0;
}
