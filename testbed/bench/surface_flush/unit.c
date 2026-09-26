/* The C that board.asm's loom_pvs_surface_flush replaced: the row walk of
 * Loom e239e1c^:runtime/src/surface.c:341-417 (loom_surface_build_frame
 * after its early-outs) with the job append it called,
 * e239e1c^:runtime/src/frame-build.c:158-174 (loom_frame_build_add_dma).
 *
 * Cut out as a function with the assembly routine's name and interface:
 * surface.c fills a LoomSurfaceFlush record (e239e1c:runtime/src/
 * surface.c:371-416), so the scene's fields, the dirty-span tables
 * (loom_surface_first/last, LOOM_SURFACE_ROWS_MAX bytes a surface), the
 * surface state (pending, cursor, next job id) and the frame build's free
 * job slots (jobs, job_room) come from the record, and what the walk leaves
 * (cursor, pending, next job id, jobs and bytes written) goes back into it.
 * The frame-open and null checks of loom_frame_build_add_dma are left out;
 * its capacity check counts down a copy of the record's job_room (the
 * frame build's free slots, which the caller advances by jobs_written). */
#include "loom_surface.h"

#define LOOM_SURFACE_CLEAN ((loom_u8)0xffu)
#define LOOM_SURFACE_JOB_ID_BASE ((loom_u16)0x4000u)

static LoomStatus loom_frame_build_add_dma(loom_u16 *room, LoomDmaJob **next,
                                           const LoomDmaJob *job)
{
    if (*room == 0u) {
        return LOOM_STATUS_CAPACITY;
    }
    **next = *job;
    ++*next;
    --*room;
    return LOOM_STATUS_OK;
}

loom_u16 loom_pvs_surface_flush(LoomSurfaceFlush *flush)
{
    const LoomSurfaceSpec *spec;
    LoomDmaJob job;
    LoomDmaJob *next;
    loom_u16 room;
    LoomStatus status;
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

    next = flush->jobs;
    room = flush->job_room;
    status = LOOM_STATUS_OK;
    rows_left = 0u;
    for (index = 0u; index < flush->surface_count; ++index) {
        rows_left = (loom_u16)(rows_left + flush->specs[index].height);
    }
    bytes = 0u;
    jobs = 0u;
    surface = (loom_u8)flush->cursor_surface;
    row = (loom_u8)flush->cursor_row;
    while (rows_left != 0u && flush->pending != 0u) {
        spec = &flush->specs[surface];
        first = flush->firsts[(loom_u16)((loom_u16)surface * LOOM_SURFACE_ROWS_MAX + row)];
        if (first != LOOM_SURFACE_CLEAN) {
            if (jobs >= flush->jobs_per_tick) {
                break;
            }
            last = flush->lasts[(loom_u16)((loom_u16)surface * LOOM_SURFACE_ROWS_MAX + row)];
            run_words = (loom_u16)((loom_u16)(last - first) + 1u);
            run_bytes = (loom_u16)(run_words * 2u);
            if (run_bytes > (loom_u16)(flush->bytes_per_tick - bytes)) {
                /* Send the cells that fit and keep the rest of the row. */
                run_words = (loom_u16)((loom_u16)(flush->bytes_per_tick - bytes) / 2u);
                if (run_words == 0u) {
                    break;
                }
                run_bytes = (loom_u16)(run_words * 2u);
            }
            job.job_id = (loom_u16)(LOOM_SURFACE_JOB_ID_BASE |
                                    (loom_u16)(flush->next_job_id & 0x3fffu));
            job.source_handle = LOOM_SURFACE_BLOCK_HANDLE;
            job.source_offset = (loom_u16)(
                (loom_u16)(spec->shadow_word_offset +
                           (loom_u16)((loom_u16)row * (loom_u16)spec->width) +
                           first) *
                2u);
            job.destination_offset = (loom_u16)(
                (loom_u16)(spec->map_word_base +
                           (loom_u16)((loom_u16)row * LOOM_SURFACE_MAP_ROW_WORDS) +
                           first) *
                2u);
            job.byte_count = run_bytes;
            job.source_kind = LOOM_DMA_SOURCE_WRAM_BLOCK;
            job.destination_kind = LOOM_DMA_DESTINATION_VRAM;
            job.policy = LOOM_DMA_REQUIRED;
            job.reserved = 0u;
            status = loom_frame_build_add_dma(&room, &next, &job);
            if (status != LOOM_STATUS_OK) {
                break;
            }
            ++flush->next_job_id;
            ++jobs;
            bytes = (loom_u16)(bytes + run_bytes);
            flush->pending = (loom_u16)(flush->pending - run_words);
            if (run_words == (loom_u16)((loom_u16)(last - first) + 1u)) {
                flush->firsts[(loom_u16)((loom_u16)surface * LOOM_SURFACE_ROWS_MAX + row)] =
                    LOOM_SURFACE_CLEAN;
            } else {
                /* The budget is spent; the cursor stays on this row. */
                flush->firsts[(loom_u16)((loom_u16)surface * LOOM_SURFACE_ROWS_MAX + row)] =
                    (loom_u8)(first + (loom_u8)run_words);
                break;
            }
        }
        ++row;
        if (row >= spec->height) {
            row = 0u;
            ++surface;
            if (surface >= flush->surface_count) {
                surface = 0u;
            }
        }
        --rows_left;
    }
    flush->cursor_surface = surface;
    flush->cursor_row = row;
    flush->bytes_written = bytes;
    flush->jobs_written = jobs;
    return status;
}
