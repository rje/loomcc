// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: mode1-helpers.expected-output
// loomcc-asm-sources: mode1-helpers.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/mode1.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: Mode 1 scroll glue: clamping, parallax scaling by a
// numerator/denominator (shifts for powers of two, division otherwise),
// wrapped auto-scroll drift and the stream window limits.
#include "../loom-d88b68b/runtime/src/mode1.c"
int printf(const char *fmt, ...);
int main(void) {
  static const loom_s16 vals[] = { 0, 1, -1, 7, -7, 255, -256, 1000, -1000, 32767, -32767 };
  static const loom_s16 drifts[] = { 0, 1, 255, 256, 300, -1, -300, 1024, -1024 };
  LoomMode1Stream st;
  loom_u8 i, n, d, t;
  for (i = 0; i < 11; i++) {
    printf("%d: c%d", vals[i], loom_mode1_clamp(vals[i], -100, 300));
    for (n = 1; n <= 3; n++)
      for (d = 1; d <= 8; d += (d < 4 ? 1 : 4)) printf(" %d", loom_mode1_scaled(vals[i], n, d));
    printf("\n");
  }
  for (i = 0; i < 9; i++) {
    loom_s16 px = 5, frac = 0;
    for (t = 0; t < 7; t++) loom_mode1_advance_auto_scroll(drifts[i], &px, &frac, 512);
    printf("drift %d -> %d.%d\n", drifts[i], px, frac);
  }
  for (i = 0; i < 5; i++) {
    st.width_metatiles = (loom_u16)(i * 12); st.height_metatiles = (loom_u16)(i * 9);
    printf("stream %u %u -> %u %u\n", st.width_metatiles, st.height_metatiles, loom_mode1_stream_max_column(&st), loom_mode1_stream_max_row(&st));
  }
  return 0;
}
