// loomcc-do: run
// loomcc-int: 16
// loomcc-options: -I../loom-d88b68b/runtime/include -I../loom-d88b68b/runtime/backends/pvsneslib/include -I../loom-d88b68b/examples/cliffside/.loom/generated/include -I%PVSNESLIB%/pvsneslib/include -I%PVSNESLIB%/devkitsnes/include -DLOOM_BUILD_DEBUG=1 -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
// loomcc-expect-output: frame-shell.expected-output
// loomcc-asm-sources: ../loom-d88b68b/runtime/backends/pvsneslib/src/vblank.asm ../loom-d88b68b/runtime/backends/pvsneslib/src/oam.asm frame-shell.stubs.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// loomcc-max-frames: 3600
// loomcc-source: Loom d88b68b runtime/backends/pvsneslib/src/runtime-adapter.c, frame-transaction.c, vblank.asm and oam.asm (see ../loom-d88b68b/PROVENANCE)
// T6 stage 2: the PVSnesLib frame shell with NMI running. Commits are
// opened, validated and published through frame-transaction.c and Loom's
// oam.asm; loom_port_frame_wait passes VBlanks to the tick cadence, waits,
// presents through vblank.asm (DMA of the stream block, display registers)
// and fills the boundary and input snapshot. Printed: each boundary's frame
// id, presented commit, presentation kind and missed count, and whether the
// tick took its two VBlanks; plus the display validation over bad states.
#include "../loom-d88b68b/runtime/backends/pvsneslib/src/frame-transaction.c"
#include "../loom-d88b68b/runtime/backends/pvsneslib/src/runtime-adapter.c"
int printf(const char *fmt, ...);

const loom_u8 loom_pvs_generated_tick_frames = 2;
static loom_u8 block[64];
loom_u8 *loom_mode1_stream_block(loom_u16 *bytes) { *bytes = sizeof block; return block; }

static LoomFrameCommit commits[3];

static void good(LoomFrameCommit *c, LoomCommitId id) {
  loom_u8 i;
  for (i = 0; i < 4; i++) { c->display.bg_scroll_x[i] = (loom_s16)(i * 3); c->display.bg_scroll_y[i] = (loom_s16)(-i); }
  c->commit_id = id;
  c->display.mode = LOOM_DISPLAY_MODE_1;
  c->display.brightness = 15;
  c->display.main_layers = 0x11;
  c->display.backdrop_color = 0x1234;
  c->raster.program = LOOM_RASTER_PROGRAM_NONE;
  c->raster.state = LOOM_RASTER_STATE_NONE;
}

int main(void) {
  LoomFrameBoundary frame;
  LoomInputSnapshot input;
  LoomFrameCommit bad;
  LoomDmaJob job;
  loom_u8 i, f;
  loom_u16 before;

  /* Display validation. */
  good(&bad, 1);
  printf("valid %u", (unsigned)loom_pvs_target_commit_supported(&bad));
  for (i = 0; i < 12; i++) {
    good(&bad, 1);
    switch (i) {
    case 0: bad.display.reserved = 1; break;
    case 1: bad.display.mode = 0; break;
    case 2: bad.display.brightness = 16; break;
    case 3: bad.display.main_layers = 0x20; break;
    case 4: bad.display.obj_size_pair = 9; break;
    case 5: bad.display.mosaic_size = 1; break;
    case 6: bad.display.mosaic_layers = 1; break;
    case 7: bad.display.mosaic_size = 4; bad.display.mosaic_layers = 3; break;
    case 8: bad.display.flags = 4; break;
    case 9: bad.display.backdrop_color = 0x8000u; break;
    case 10: bad.raster.state = 0; break;
    case 11: bad.raster.program = 7; break;
    }
    printf(" %u", (unsigned)loom_pvs_target_commit_supported(&bad));
  }
  printf("\n");
  job.source_kind = LOOM_DMA_SOURCE_WRAM_BLOCK;
  job.source_handle = LOOM_MODE1_STREAM_BLOCK_HANDLE;
  for (i = 0; i < 5; i++) {
    static const loom_u16 offs[5] = { 0, 32, 64, 60, 65 };
    static const loom_u16 counts[5] = { 64, 32, 0, 8, 0 };
    job.source_offset = offs[i]; job.byte_count = counts[i];
    printf("%u", (unsigned)loom_pvs_target_dma_source_valid(&job));
  }
  printf("\n");

  /* The frame shell. */
  for (i = 0; i < 64; i++) block[i] = (loom_u8)(i * 5u);
  loom_pvs_frame_transaction_initialize();
  loom_pvs_runtime_initialized = LOOM_TRUE;
  printf("wait before init: %u\n", (unsigned)loom_port_frame_wait((LoomFrameBoundary *)0, &input));
  REG_NMITIMEN = 0x80;   /* NMI on */
  loom_pvs_runtime_tick_start_vblanks = snes_vblank_count;
  for (f = 0; f < 10; f++) {
    LoomStatus st = LOOM_STATUS_OK;
    if (f % 3 != 2) {
      LoomFrameCommit *c = &commits[f % 3];
      good(c, (LoomCommitId)(100 + f));
      c->dma_count = (loom_u8)(f & 1);
      st = loom_port_frame_begin(c);
      if (st == LOOM_STATUS_OK && (f & 1)) {
        job.job_id = f; job.source_offset = 0; job.byte_count = 64;
        job.destination_offset = 0x7000u; job.destination_kind = LOOM_DMA_DESTINATION_VRAM;
        job.policy = LOOM_DMA_REQUIRED; job.reserved = 0;
        st = loom_port_dma_stage(&job);
      }
      if (st == LOOM_STATUS_OK) st = loom_port_frame_commit();
      if (f == 4) printf("busy %u\n", (unsigned)loom_port_frame_begin(c));
    }
    before = snes_vblank_count;
    loom_port_frame_wait(&frame, &input);
    printf("f%u st%u id%u commit%u pres%u missed%u pads%u %s\n", (unsigned)f, (unsigned)st,
           (unsigned)frame.frame_id, (unsigned)frame.presented_commit_id, (unsigned)frame.presentation,
           (unsigned)frame.missed_commit_count, (unsigned)input.pad_count,
           (loom_u16)(snes_vblank_count - before) >= 1u && (loom_u16)(snes_vblank_count - before) <= 3u ? "ok" : "late");
  }
  REG_NMITIMEN = 0x00;
  return 0;
}
