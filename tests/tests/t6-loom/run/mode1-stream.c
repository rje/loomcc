// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: mode1-stream.expected-output
// loomcc-asm-sources: mode1-stream.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/src/mode1.c (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: Mode 1's map streaming. A 70x40 metatile grid (with blank
// and out-of-range ids) streams as the camera walks right, down, back left
// and up: the window origin, the column and row strips expanded into the
// stream buffer, and the DMA jobs they queue (captured by a fake
// loom_frame_build_add_dma), compared with the 816-tcc build.
#include "../loom-d88b68b/runtime/src/mode1.c"
int printf(const char *fmt, ...);

static loom_u16 jobs, job_hash, fail_after = 0xffffu;
LoomStatus loom_frame_build_add_dma(const LoomDmaJob *job) {
  if (jobs == fail_after) return LOOM_STATUS_NOT_READY;
  ++jobs;
  job_hash = (loom_u16)(1u * job_hash * 31u + job->job_id);
  job_hash = (loom_u16)(1u * job_hash * 31u + job->source_offset);
  job_hash = (loom_u16)(1u * job_hash * 31u + job->destination_offset);
  job_hash = (loom_u16)(1u * job_hash * 31u + job->byte_count);
  job_hash = (loom_u16)(1u * job_hash * 31u + job->destination_kind + job->source_kind * 7u + job->policy * 13u + job->source_handle);
  return LOOM_STATUS_OK;
}

#define W 70
#define H 40
static loom_u8 grid[W * H];
static loom_u16 table[4 * 12];
static LoomMode1Stream stream;
static LoomMode1Scene scene;

static loom_u16 buffer_hash(void) {
  loom_u16 i, h = 0;
  for (i = 0; i < LOOM_MODE1_STREAM_BUFFER_WORDS; i++) h = (loom_u16)(1u * h * 33u + loom_mode1_stream_buffer[i]);
  return h;
}

static void step(loom_s16 cx, loom_s16 cy) {
  LoomStatus st;
  loom_mode1_state.camera_x = cx;
  loom_mode1_state.camera_y = cy;
  st = loom_mode1_stream_update();
  printf("%d,%d =%u c%u r%u j%u %x %x\n", cx, cy, (unsigned)st, loom_mode1_state.stream_column,
         loom_mode1_state.stream_row, jobs, job_hash, buffer_hash());
}

int main(void) {
  loom_u16 i;
  loom_s16 x, y;
  for (i = 0; i < W * H; i++) {
    loom_u16 v = (loom_u16)(1u * i * 7u + i / W);
    grid[i] = (loom_u8)(v % 17u == 0u ? LOOM_MODE1_STREAM_BLANK : v % 13u);  /* 12 is out of range */
  }
  for (i = 0; i < 4 * 12; i++) table[i] = (loom_u16)(0x2000u + i * 0x111u);
  stream.width_metatiles = W; stream.height_metatiles = H;
  stream.blank_word = 0x00ffu; stream.metatile_count = 12;
  stream.grid = grid; stream.table = table;
  scene.stream = &stream;
  loom_mode1_state.scene = &scene;
  printf("max %u %u\n", loom_mode1_stream_max_column(&stream), loom_mode1_stream_max_row(&stream));
  for (x = 0; x <= 700; x += 40) step(x, 0);
  for (y = 0; y <= 500; y += 24) step(700, y);
  for (x = 700; x >= 500; x -= 50) step(x, 500);
  for (y = 500; y >= 380; y -= 30) step(500, y);
  step(-40, -40);
  fail_after = (loom_u16)(jobs + 1u);
  step(900, 900);
  step(900, 900);
  scene.stream = (const LoomMode1Stream *)0;
  step(0, 0);
  return 0;
}
