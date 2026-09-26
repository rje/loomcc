#include <loom/surface.h>

#include <loom/frame.h>

#define LOOM_SURFACE_CLEAN ((loom_u8)0xffu)
/* Surface job ids sit above the map stream's 0x8000 range and the loads. */
#define LOOM_SURFACE_JOB_ID_BASE ((loom_u16)0x4000u)

typedef struct LoomSurfaceState {
    const LoomSurfaceScene *scene;
    loom_u16 pending_cells;
    loom_u16 last_bytes;
    loom_u16 next_job_id;
    loom_u8 last_jobs;
    loom_u8 cursor_surface;
    loom_u8 cursor_row;
    loom_u8 initialized;
} LoomSurfaceState;

static LoomSurfaceState loom_surface_state;
/* Exported, not static: a ROM test can read what a surface will show and
 * which rows are still waiting to reach VRAM, by symbol. */
loom_u16 loom_surface_shadow[LOOM_SURFACE_WORD_CAPACITY];
/* Per row: the first dirty column, or LOOM_SURFACE_CLEAN, and the last. */
loom_u8 loom_surface_first[LOOM_SURFACE_CAPACITY][LOOM_SURFACE_ROWS_MAX];
loom_u8 loom_surface_last[LOOM_SURFACE_CAPACITY][LOOM_SURFACE_ROWS_MAX];
#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
LOOM_STATIC_ASSERT(loom_surface_flush_matches_board_asm,
                   sizeof(LoomSurfaceFlush) == LOOM_SURFACE_FLUSH_BYTES);
LOOM_STATIC_ASSERT(loom_surface_spec_matches_board_asm,
                   sizeof(LoomSurfaceSpec) == LOOM_SURFACE_SPEC_BYTES);
LOOM_STATIC_ASSERT(loom_dma_job_matches_board_asm,
                   sizeof(LoomDmaJob) == LOOM_DMA_JOB_BYTES);
LOOM_STATIC_ASSERT(loom_surface_rows_match_board_asm, LOOM_SURFACE_ROWS_MAX == 32u);
static LoomSurfaceFlush loom_surface_flush_record;
#endif

static void loom_surface_clear_dirty(void)
{
    loom_u8 surface;
    loom_u8 row;

    for (surface = 0u; surface < LOOM_SURFACE_CAPACITY; ++surface) {
        for (row = 0u; row < LOOM_SURFACE_ROWS_MAX; ++row) {
            loom_surface_first[surface][row] = LOOM_SURFACE_CLEAN;
            loom_surface_last[surface][row] = 0u;
        }
    }
    loom_surface_state.pending_cells = 0u;
    loom_surface_state.cursor_surface = 0u;
    loom_surface_state.cursor_row = 0u;
}

LoomStatus loom_surface_initialize(void)
{
    loom_surface_state.scene = (const LoomSurfaceScene *)0;
    loom_surface_state.last_bytes = 0u;
    loom_surface_state.last_jobs = 0u;
    loom_surface_state.next_job_id = 0u;
    loom_surface_state.initialized = LOOM_TRUE;
    loom_surface_clear_dirty();
    return LOOM_STATUS_OK;
}

static LoomStatus loom_surface_validate(const LoomSurfaceScene *scene)
{
    const LoomSurfaceSpec *spec;
    loom_u8 index;
    loom_u16 words;
    loom_u16 map_end;

    if (scene->surface_count > LOOM_SURFACE_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
    if (scene->surface_count == 0u) {
        return LOOM_STATUS_OK;
    }
    if (scene->surfaces == (const LoomSurfaceSpec *)0 ||
        scene->jobs_per_tick == 0u ||
        scene->jobs_per_tick > LOOM_FRAME_DMA_CAPACITY ||
        scene->bytes_per_tick < 2u ||
        scene->bytes_per_tick > LOOM_FRAME_REQUIRED_DMA_BYTES_MAX) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < scene->surface_count; ++index) {
        spec = &scene->surfaces[index];
        if (spec->width == 0u || spec->width > LOOM_SURFACE_COLUMNS_MAX ||
            spec->height == 0u || spec->height > LOOM_SURFACE_ROWS_MAX) {
            return LOOM_STATUS_OUT_OF_RANGE;
        }
        words = (loom_u16)((loom_u16)spec->width * (loom_u16)spec->height);
        if (spec->shadow_word_offset > LOOM_SURFACE_WORD_CAPACITY ||
            words > (loom_u16)(LOOM_SURFACE_WORD_CAPACITY -
                               spec->shadow_word_offset)) {
            return LOOM_STATUS_CAPACITY;
        }
        /* Every run's destination must stay within the 64 KiB of VRAM the
         * job's byte offset can name. */
        map_end = (loom_u16)((loom_u16)(spec->height - 1u) *
                             LOOM_SURFACE_MAP_ROW_WORDS);
        map_end = (loom_u16)(map_end + spec->width);
        if (spec->map_word_base > 0x8000u ||
            map_end > (loom_u16)(0x8000u - spec->map_word_base)) {
            return LOOM_STATUS_OUT_OF_RANGE;
        }
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_surface_activate_scene(const LoomSurfaceScene *scene)
{
    const LoomSurfaceSpec *spec;
    LoomStatus status;
    loom_u8 index;
    loom_u16 words;
    loom_u16 word;
    loom_u16 *shadow;

    if (loom_surface_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (scene == (const LoomSurfaceScene *)0) {
        loom_surface_state.scene = scene;
        loom_surface_clear_dirty();
        return LOOM_STATUS_OK;
    }
    status = loom_surface_validate(scene);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    for (index = 0u; index < scene->surface_count; ++index) {
        spec = &scene->surfaces[index];
        words = (loom_u16)((loom_u16)spec->width * (loom_u16)spec->height);
        shadow = &loom_surface_shadow[spec->shadow_word_offset];
        if (spec->initial != (const loom_u16 *)0) {
            for (word = 0u; word < words; ++word) {
                shadow[word] = spec->initial[word];
            }
        } else {
            for (word = 0u; word < words; ++word) {
                shadow[word] = spec->blank_word;
            }
        }
    }
    loom_surface_state.scene = scene;
    loom_surface_clear_dirty();
    return LOOM_STATUS_OK;
}

static LoomStatus loom_surface_locate(loom_u8 surface, loom_u8 x, loom_u8 y,
                                      const LoomSurfaceSpec **spec)
{
    const LoomSurfaceScene *scene;

    if (loom_surface_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    scene = loom_surface_state.scene;
    if (scene == (const LoomSurfaceScene *)0 ||
        surface >= scene->surface_count) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    *spec = &scene->surfaces[surface];
    if (x >= (*spec)->width || y >= (*spec)->height) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    return LOOM_STATUS_OK;
}

/* Widens row y's span to cover cells x .. x + count - 1. */
static void loom_surface_dirty(loom_u8 surface, loom_u8 y, loom_u8 x,
                               loom_u8 count)
{
    loom_u8 end;

    end = (loom_u8)(x + count - 1u);
    if (loom_surface_first[surface][y] == LOOM_SURFACE_CLEAN) {
        loom_surface_first[surface][y] = x;
        loom_surface_last[surface][y] = end;
        loom_surface_state.pending_cells =
            (loom_u16)(loom_surface_state.pending_cells + count);
        return;
    }
    if (x < loom_surface_first[surface][y]) {
        loom_surface_state.pending_cells = (loom_u16)(
            loom_surface_state.pending_cells +
            (loom_u16)(loom_surface_first[surface][y] - x));
        loom_surface_first[surface][y] = x;
    }
    if (end > loom_surface_last[surface][y]) {
        loom_surface_state.pending_cells = (loom_u16)(
            loom_surface_state.pending_cells +
            (loom_u16)(end - loom_surface_last[surface][y]));
        loom_surface_last[surface][y] = end;
    }
}

static loom_u16 *loom_surface_cell(const LoomSurfaceSpec *spec, loom_u8 x,
                                   loom_u8 y)
{
    return &loom_surface_shadow[(loom_u16)(
        spec->shadow_word_offset +
        (loom_u16)((loom_u16)y * (loom_u16)spec->width) + x)];
}

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
LoomStatus loom_surface_paint_binding(loom_u8 surface, loom_u16 **shadow,
                                      loom_u8 **first, loom_u8 **last,
                                      loom_u16 **pending, loom_u16 *stride)
{
    const LoomSurfaceSpec *spec;
    LoomStatus status;

    status = loom_surface_locate(surface, 0u, 0u, &spec);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    *shadow = &loom_surface_shadow[spec->shadow_word_offset];
    *first = loom_surface_first[surface];
    *last = loom_surface_last[surface];
    *pending = &loom_surface_state.pending_cells;
    *stride = spec->width;
    return LOOM_STATUS_OK;
}
#endif

LoomStatus loom_surface_write(loom_u8 surface, loom_u8 x, loom_u8 y,
                              loom_u16 word)
{
    const LoomSurfaceSpec *spec;
    LoomStatus status;

    status = loom_surface_locate(surface, x, y, &spec);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    *loom_surface_cell(spec, x, y) = word;
    loom_surface_dirty(surface, y, x, 1u);
    return LOOM_STATUS_OK;
}

LoomStatus loom_surface_fill(loom_u8 surface, loom_u8 x, loom_u8 y,
                             loom_u8 width, loom_u8 height, loom_u16 word)
{
    const LoomSurfaceSpec *spec;
    LoomStatus status;
    loom_u16 *cell;
    loom_u8 row;
    loom_u8 column;

    status = loom_surface_locate(surface, x, y, &spec);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    if (width == 0u || height == 0u ||
        width > (loom_u8)(spec->width - x) ||
        height > (loom_u8)(spec->height - y)) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    for (row = 0u; row < height; ++row) {
        cell = loom_surface_cell(spec, x, (loom_u8)(y + row));
        for (column = 0u; column < width; ++column) {
            cell[column] = word;
        }
        loom_surface_dirty(surface, (loom_u8)(y + row), x, width);
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_surface_write_metatile(loom_u8 surface, loom_u8 cell_x,
                                       loom_u8 cell_y,
                                       const loom_u16 *words)
{
    const LoomSurfaceSpec *spec;
    LoomStatus status;
    loom_u16 *cell;
    loom_u8 x;
    loom_u8 y;

    if (words == (const loom_u16 *)0 || cell_x > 15u || cell_y > 15u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    x = (loom_u8)(cell_x * 2u);
    y = (loom_u8)(cell_y * 2u);
    status = loom_surface_locate(surface, x, y, &spec);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    if ((loom_u8)(x + 1u) >= spec->width || (loom_u8)(y + 1u) >= spec->height) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    cell = loom_surface_cell(spec, x, y);
    cell[0] = words[0];
    cell[1] = words[1];
    cell = loom_surface_cell(spec, x, (loom_u8)(y + 1u));
    cell[0] = words[2];
    cell[1] = words[3];
    loom_surface_dirty(surface, y, x, 2u);
    loom_surface_dirty(surface, (loom_u8)(y + 1u), x, 2u);
    return LOOM_STATUS_OK;
}

LoomStatus loom_surface_read(loom_u8 surface, loom_u8 x, loom_u8 y,
                             loom_u16 *word)
{
    const LoomSurfaceSpec *spec;
    LoomStatus status;

    if (word == (loom_u16 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    status = loom_surface_locate(surface, x, y, &spec);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    *word = *loom_surface_cell(spec, x, y);
    return LOOM_STATUS_OK;
}

loom_u16 *loom_surface_row(loom_u8 surface, loom_u8 y)
{
    const LoomSurfaceSpec *spec;

    if (loom_surface_locate(surface, 0u, y, &spec) != LOOM_STATUS_OK) {
        return (loom_u16 *)0;
    }
    return loom_surface_cell(spec, 0u, y);
}

LoomStatus loom_surface_mark(loom_u8 surface, loom_u8 x, loom_u8 y,
                             loom_u8 count)
{
    const LoomSurfaceSpec *spec;
    LoomStatus status;

    status = loom_surface_locate(surface, x, y, &spec);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    if (count == 0u || count > (loom_u8)(spec->width - x)) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    loom_surface_dirty(surface, y, x, count);
    return LOOM_STATUS_OK;
}

LoomStatus loom_surface_build_frame(void)
{
    const LoomSurfaceScene *scene;
    const LoomSurfaceSpec *spec;
    LoomDmaJob *job;
    loom_u16 bytes;
    loom_u16 rows_left;
    loom_u16 run_words;
    loom_u16 run_bytes;
    loom_u8 jobs;
    loom_u8 surface;
    loom_u8 row;
    loom_u8 first;
    loom_u8 last;
    loom_u8 index;

    loom_surface_state.last_bytes = 0u;
    loom_surface_state.last_jobs = 0u;
    if (loom_surface_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    scene = loom_surface_state.scene;
    if (scene == (const LoomSurfaceScene *)0 || scene->surface_count == 0u ||
        loom_surface_state.pending_cells == 0u) {
        return LOOM_STATUS_OK;
    }
#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
    {
        /* board.asm walks the rows and writes the jobs in place: through
         * 816-tcc the walk cost about eight scanlines a dirty row. */
        LoomSurfaceFlush *flush;
        loom_u8 room;
        LoomStatus status;

        flush = &loom_surface_flush_record;
        (void)index;
        (void)spec;
        (void)job;
        (void)bytes;
        (void)rows_left;
        (void)run_words;
        (void)run_bytes;
        (void)jobs;
        (void)surface;
        (void)row;
        (void)first;
        (void)last;
        flush->jobs = loom_frame_build_dma_space(&room);
        if (flush->jobs == (LoomDmaJob *)0) {
            return LOOM_STATUS_NOT_READY;
        }
        flush->specs = scene->surfaces;
        flush->firsts = &loom_surface_first[0][0];
        flush->lasts = &loom_surface_last[0][0];
        flush->job_room = room;
        flush->surface_count = scene->surface_count;
        flush->jobs_per_tick = scene->jobs_per_tick;
        flush->bytes_per_tick = scene->bytes_per_tick;
        flush->pending = loom_surface_state.pending_cells;
        flush->cursor_surface = loom_surface_state.cursor_surface;
        flush->cursor_row = loom_surface_state.cursor_row;
        flush->next_job_id = loom_surface_state.next_job_id;
        status = (LoomStatus)loom_pvs_surface_flush(flush);
        loom_frame_build_dma_advance((loom_u8)flush->jobs_written);
        loom_surface_state.pending_cells = flush->pending;
        loom_surface_state.cursor_surface = (loom_u8)flush->cursor_surface;
        loom_surface_state.cursor_row = (loom_u8)flush->cursor_row;
        loom_surface_state.next_job_id = flush->next_job_id;
        loom_surface_state.last_jobs = (loom_u8)flush->jobs_written;
        loom_surface_state.last_bytes = flush->bytes_written;
        return status;
    }
#else
    rows_left = 0u;
    for (index = 0u; index < scene->surface_count; ++index) {
        rows_left = (loom_u16)(rows_left + scene->surfaces[index].height);
    }
    bytes = 0u;
    jobs = 0u;
    surface = loom_surface_state.cursor_surface;
    row = loom_surface_state.cursor_row;
    while (rows_left != 0u && loom_surface_state.pending_cells != 0u) {
        spec = &scene->surfaces[surface];
        first = loom_surface_first[surface][row];
        if (first != LOOM_SURFACE_CLEAN) {
            if (jobs >= scene->jobs_per_tick) {
                break;
            }
            last = loom_surface_last[surface][row];
            run_words = (loom_u16)((loom_u16)(last - first) + 1u);
            run_bytes = (loom_u16)(run_words * 2u);
            if (run_bytes > (loom_u16)(scene->bytes_per_tick - bytes)) {
                /* Send the cells that fit and keep the rest of the row. */
                run_words = (loom_u16)((loom_u16)(scene->bytes_per_tick - bytes) / 2u);
                if (run_words == 0u) {
                    break;
                }
                run_bytes = (loom_u16)(run_words * 2u);
            }
            /* Written in place in the frame's job list: a local job and a
             * copy cost the flush several scanlines a row on 816-tcc. */
            job = loom_frame_build_reserve_dma();
            if (job == (LoomDmaJob *)0) {
                return LOOM_STATUS_CAPACITY;
            }
            job->job_id = (loom_u16)(LOOM_SURFACE_JOB_ID_BASE |
                                     (loom_u16)(loom_surface_state.next_job_id & 0x3fffu));
            job->source_handle = LOOM_SURFACE_BLOCK_HANDLE;
            job->source_offset = (loom_u16)(
                (loom_u16)(spec->shadow_word_offset +
                           (loom_u16)((loom_u16)row * (loom_u16)spec->width) +
                           first) *
                2u);
            job->destination_offset = (loom_u16)(
                (loom_u16)(spec->map_word_base +
                           (loom_u16)((loom_u16)row * LOOM_SURFACE_MAP_ROW_WORDS) +
                           first) *
                2u);
            job->byte_count = run_bytes;
            job->source_kind = LOOM_DMA_SOURCE_WRAM_BLOCK;
            job->destination_kind = LOOM_DMA_DESTINATION_VRAM;
            job->policy = LOOM_DMA_REQUIRED;
            job->reserved = 0u;
            ++loom_surface_state.next_job_id;
            ++jobs;
            bytes = (loom_u16)(bytes + run_bytes);
            loom_surface_state.pending_cells =
                (loom_u16)(loom_surface_state.pending_cells - run_words);
            if (run_words == (loom_u16)((loom_u16)(last - first) + 1u)) {
                loom_surface_first[surface][row] = LOOM_SURFACE_CLEAN;
            } else {
                /* The budget is spent; the cursor stays on this row. */
                loom_surface_first[surface][row] =
                    (loom_u8)(first + (loom_u8)run_words);
                break;
            }
        }
        ++row;
        if (row >= spec->height) {
            row = 0u;
            ++surface;
            if (surface >= scene->surface_count) {
                surface = 0u;
            }
        }
        --rows_left;
    }
    loom_surface_state.cursor_surface = surface;
    loom_surface_state.cursor_row = row;
    loom_surface_state.last_bytes = bytes;
    loom_surface_state.last_jobs = jobs;
    return LOOM_STATUS_OK;
#endif
}

loom_u16 loom_surface_pending_cells(void)
{
    return loom_surface_state.pending_cells;
}

loom_u16 loom_surface_last_bytes(void)
{
    return loom_surface_state.last_bytes;
}

loom_u8 loom_surface_last_jobs(void)
{
    return loom_surface_state.last_jobs;
}

loom_u8 *loom_surface_block(loom_u16 *bytes)
{
    if (bytes != (loom_u16 *)0) {
        *bytes = (loom_u16)(LOOM_SURFACE_WORD_CAPACITY * 2u);
    }
    return (loom_u8 *)loom_surface_shadow;
}
