/* surface_flush: the frame build's surface pass for Stack's two surfaces
 * (the 10x20-tile well and the 4x4 next-piece box). The tick's dirty rows:
 * the rows a falling piece crossed, four full rows redrawn by a line clear
 * and the preview box. Two ticks' flushes from the cursor (surface 0, row
 * 2) with a budget of 4 jobs and 50 bytes a tick, so the first tick stops
 * on the job limit and the second splits a row on the byte budget (the
 * rest of the backlog waits for later ticks);
 * between them the driver moves the job list on as the frame build would.
 * bench_check folds the jobs written, the dirty spans and the record. */
#include "bench.h"
#include "loom_surface.h"

loom_u16 loom_pvs_surface_flush(LoomSurfaceFlush *flush);

#define SURFACES 4
#define JOBS 16

static LoomSurfaceSpec specs[2];
static loom_u8 firsts[SURFACES * 32];
static loom_u8 lasts[SURFACES * 32];
static LoomDmaJob jobs[JOBS];
static LoomSurfaceFlush flush;
static loom_u16 status[2];
static loom_u16 written[4];

static void dirty(loom_u16 surface, loom_u16 row, loom_u8 first, loom_u8 last)
{
    firsts[surface * 32 + row] = first;
    lasts[surface * 32 + row] = last;
    flush.pending = (loom_u16)(flush.pending + (last - first) + 1u);
}

void bench_setup(void)
{
    unsigned short i;
    specs[0].map_word_base = 0x5846u; /* screen row 2, column 6 */
    specs[0].shadow_word_offset = 0;
    specs[0].blank_word = 0x2400u;
    specs[0].width = 10;
    specs[0].height = 20;
    specs[0].initial = 0;
    specs[1].map_word_base = 0x5876u; /* screen row 3, column 22 */
    specs[1].shadow_word_offset = 200;
    specs[1].blank_word = 0x2400u;
    specs[1].width = 4;
    specs[1].height = 4;
    specs[1].initial = 0;
    for (i = 0; i < SURFACES * 32; i++) {
        firsts[i] = 0xff;
        lasts[i] = 0;
    }
    for (i = 0; i < JOBS; i++) {
        jobs[i].job_id = 0xeeee;
        jobs[i].source_handle = 0xeeee;
        jobs[i].source_offset = 0xeeee;
        jobs[i].destination_offset = 0xeeee;
        jobs[i].byte_count = 0xeeee;
        jobs[i].source_kind = 0xee;
        jobs[i].destination_kind = 0xee;
        jobs[i].policy = 0xee;
        jobs[i].reserved = 0xee;
    }
    flush.pending = 0;
    dirty(0, 1, 3, 5); /* behind the cursor: waits for the wrap */
    dirty(0, 4, 4, 4);
    dirty(0, 5, 3, 5);
    dirty(0, 6, 4, 6);
    dirty(0, 12, 2, 3);
    for (i = 16; i < 20; i++)
        dirty(0, i, 0, 9); /* a four-row clear */
    dirty(1, 0, 0, 3);
    dirty(1, 1, 1, 2);
    flush.specs = specs;
    flush.firsts = firsts;
    flush.lasts = lasts;
    flush.jobs = jobs;
    flush.job_room = JOBS;
    flush.surface_count = 2;
    flush.jobs_per_tick = 4;
    flush.bytes_per_tick = 50;
    flush.cursor_surface = 0;
    flush.cursor_row = 2;
    flush.next_job_id = 0x3ffe; /* wraps inside the 14-bit id */
    flush.jobs_written = 0;
    flush.bytes_written = 0;
}

void bench_run(void)
{
    status[0] = loom_pvs_surface_flush(&flush);
    written[0] = flush.jobs_written;
    written[1] = flush.bytes_written;
    flush.jobs = &jobs[4];
    flush.job_room = 12;
    status[1] = loom_pvs_surface_flush(&flush);
    written[2] = flush.jobs_written;
    written[3] = flush.bytes_written;
}

void bench_check(void)
{
    unsigned short i, h;
    const loom_u8 *b;
    BENCH_OUT(status[0]);
    BENCH_OUT(status[1]);
    BENCH_OUT(written[0]);
    BENCH_OUT(written[1]);
    BENCH_OUT(written[2]);
    BENCH_OUT(written[3]);
    BENCH_OUT(flush.pending);
    BENCH_OUT(flush.cursor_surface);
    BENCH_OUT(flush.cursor_row);
    BENCH_OUT(flush.next_job_id);
    BENCH_OUT(flush.job_room);
    h = 5381u;
    for (i = 0; i < SURFACES * 32; i++)
        h = (loom_u16)((loom_u16)(h << 5) + h + firsts[i]);
    BENCH_OUT(h);
    h = 5381u;
    for (i = 0; i < SURFACES * 32; i++)
        h = (loom_u16)((loom_u16)(h << 5) + h + lasts[i]);
    BENCH_OUT(h);
    for (i = 0; i < JOBS; i++) {
        h = (loom_u16)(jobs[i].job_id ^ (jobs[i].source_handle << 3) ^
                       (jobs[i].source_offset * 5u) ^ (jobs[i].destination_offset * 7u) ^
                       (jobs[i].byte_count << 9) ^ jobs[i].source_kind ^
                       (jobs[i].destination_kind << 4) ^ (jobs[i].policy << 8) ^
                       (jobs[i].reserved << 12));
        BENCH_OUT(h);
    }
}
