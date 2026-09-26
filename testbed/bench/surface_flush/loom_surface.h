/* loomcc testbed: the Loom types the surface flush pair shares, copied from
 * Loom e239e1c (runtime/include/loom/types.h, surface.h, video.h). board.asm
 * reads LoomSurfaceFlush (36 bytes), LoomSurfaceSpec (12) and LoomDmaJob
 * (14) by offset; the sizes are asserted where pointers are four bytes. */
#ifndef LOOMCC_BENCH_LOOM_SURFACE_H
#define LOOMCC_BENCH_LOOM_SURFACE_H

typedef unsigned char loom_u8;
typedef unsigned short loom_u16;
typedef loom_u8 LoomStatus;
typedef loom_u16 LoomWramBlockHandle;

#define LOOM_STATUS_OK ((LoomStatus)0u)
#define LOOM_STATUS_CAPACITY ((LoomStatus)2u)

#define LOOM_SURFACE_BLOCK_HANDLE ((LoomWramBlockHandle)1u)
#define LOOM_SURFACE_ROWS_MAX ((loom_u8)32u)
#define LOOM_SURFACE_MAP_ROW_WORDS ((loom_u16)32u)
#define LOOM_DMA_SOURCE_WRAM_BLOCK ((loom_u8)1u)
#define LOOM_DMA_DESTINATION_VRAM ((loom_u8)0u)
#define LOOM_DMA_REQUIRED ((loom_u8)0u)

typedef struct LoomSurfaceSpec {
    loom_u16 map_word_base;
    loom_u16 shadow_word_offset;
    loom_u16 blank_word;
    loom_u8 width;
    loom_u8 height;
    const loom_u16 *initial;
} LoomSurfaceSpec;

typedef struct LoomDmaJob {
    loom_u16 job_id;
    loom_u16 source_handle;
    loom_u16 source_offset;
    loom_u16 destination_offset;
    loom_u16 byte_count;
    loom_u8 source_kind;
    loom_u8 destination_kind;
    loom_u8 policy;
    loom_u8 reserved;
} LoomDmaJob;

typedef struct LoomSurfaceFlush {
    const LoomSurfaceSpec *specs;
    loom_u8 *firsts;
    loom_u8 *lasts;
    LoomDmaJob *jobs;
    loom_u16 job_room;
    loom_u16 surface_count;
    loom_u16 jobs_per_tick;
    loom_u16 bytes_per_tick;
    loom_u16 pending;
    loom_u16 cursor_surface;
    loom_u16 cursor_row;
    loom_u16 next_job_id;
    loom_u16 jobs_written;
    loom_u16 bytes_written;
} LoomSurfaceFlush;

#define LOOM_SURFACE_FLUSH_BYTES 36u
#define LOOM_SURFACE_SPEC_BYTES 12u
#define LOOM_DMA_JOB_BYTES 14u
typedef char loom_surface_flush_matches_board_asm
    [(sizeof(void *) != 4u || sizeof(LoomSurfaceFlush) == LOOM_SURFACE_FLUSH_BYTES) ? 1 : -1];
typedef char loom_surface_spec_matches_board_asm
    [(sizeof(void *) != 4u || sizeof(LoomSurfaceSpec) == LOOM_SURFACE_SPEC_BYTES) ? 1 : -1];
typedef char loom_dma_job_matches_board_asm[sizeof(LoomDmaJob) == LOOM_DMA_JOB_BYTES ? 1 : -1];

#endif
