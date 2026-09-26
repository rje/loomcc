#ifndef LOOM_SURFACE_H
#define LOOM_SURFACE_H

#include <loom/memory.h>
#include <loom/pools.h>
#include <loom/video.h>

/*
 * Surfaces (SURF-001): a rectangle of a tile layer's hardware map whose
 * cells the game writes and the frame sends as dirty runs.
 *
 * A cell is one tilemap word (an 8x8 tile). Every surface keeps its words
 * in one WRAM shadow block and, per row, the span of columns written since
 * that row was last sent. The build-render phase turns spans into DMA jobs
 * straight from the shadow, one run per row, walking rows in order from a
 * cursor that survives the tick until the scene's budget of jobs and bytes
 * is spent; what is left waits for the next tick in the same order, so a
 * busy row never starves a later one. A run is whole cells, so a cell is
 * never presented half written, and a write made while a backlog exists
 * widens a span rather than being dropped.
 *
 * A surface lies within one 32x32 screen of its map: a map row is 32 words
 * and the surface's top-left cell is `map_word_base` words into VRAM.
 */

/* The WRAM block surface jobs source from; the map stream block is 0. */
#define LOOM_SURFACE_BLOCK_HANDLE ((LoomWramBlockHandle)1u)
#define LOOM_SURFACE_ROWS_MAX ((loom_u8)32u)
#define LOOM_SURFACE_COLUMNS_MAX ((loom_u8)32u)
#define LOOM_SURFACE_MAP_ROW_WORDS ((loom_u16)32u)

typedef struct LoomSurfaceSpec {
    /* VRAM word address of the top-left cell within the layer's map. */
    loom_u16 map_word_base;
    /* Words into the shadow block where this surface's cells start. */
    loom_u16 shadow_word_offset;
    /* The word every cell starts as when `initial` is null. */
    loom_u16 blank_word;
    loom_u8 width;
    loom_u8 height;
    /* The painted words the shadow starts from (width * height), or null:
     * VRAM already holds them from the map load, so nothing is dirty. */
    const loom_u16 *initial;
} LoomSurfaceSpec;

typedef struct LoomSurfaceScene {
    const LoomSurfaceSpec *surfaces;
    loom_u8 surface_count;
    /* The budget a presented tick may spend on surface runs, from the plan:
     * what the frame's sixteen jobs and 1,024 bytes leave beside the UI's
     * patches, tile animation and map streaming. */
    loom_u8 jobs_per_tick;
    loom_u16 bytes_per_tick;
} LoomSurfaceScene;

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
/* What board.asm writes through for a surface: its first shadow word, its
 * per-row dirty span bytes (first 0xff when clean, and last), the count of
 * dirty cells, and its width in tiles. */
/* board.asm's surface flush: surface.c fills the record, board.asm writes
 * the tick's jobs into the frame build and updates the counters. */
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
loom_u16 loom_pvs_surface_flush(LoomSurfaceFlush *flush);
LoomStatus loom_surface_paint_binding(loom_u8 surface, loom_u16 **shadow,
                                      loom_u8 **first, loom_u8 **last,
                                      loom_u16 **pending, loom_u16 *stride);
#endif
LoomStatus loom_surface_initialize(void);
/* Null clears the resident surfaces. The shadow takes each surface's initial
 * words; nothing is dirty afterwards. */
LoomStatus loom_surface_activate_scene(const LoomSurfaceScene *scene);

/* One cell, a rectangle of cells, or a 2x2 block of cells (a 16x16
 * metatile: `words` in reading order) at a cell position. */
LoomStatus loom_surface_write(loom_u8 surface, loom_u8 x, loom_u8 y,
                              loom_u16 word);
LoomStatus loom_surface_fill(loom_u8 surface, loom_u8 x, loom_u8 y,
                             loom_u8 width, loom_u8 height, loom_u16 word);
LoomStatus loom_surface_write_metatile(loom_u8 surface, loom_u8 cell_x,
                                       loom_u8 cell_y,
                                       const loom_u16 *words);
LoomStatus loom_surface_read(loom_u8 surface, loom_u8 x, loom_u8 y,
                             loom_u16 *word);
/* The fast path for a whole row: write the words in place, then mark the
 * span written. Null when the surface or row does not exist. */
loom_u16 *loom_surface_row(loom_u8 surface, loom_u8 y);
LoomStatus loom_surface_mark(loom_u8 surface, loom_u8 x, loom_u8 y,
                             loom_u8 count);

/* Adds this tick's runs to the open frame commit within the scene's budget;
 * the generated build-render phase calls it after the Mode 1 build once the
 * scene is resident. */
LoomStatus loom_surface_build_frame(void);

/* Cells in dirty spans not yet sent, and what the last build sent. */
loom_u16 loom_surface_pending_cells(void);
loom_u16 loom_surface_last_bytes(void);
loom_u8 loom_surface_last_jobs(void);

/* The shadow block surface jobs read from (LOOM_SURFACE_BLOCK_HANDLE). */
loom_u8 *loom_surface_block(loom_u16 *bytes);

#endif
