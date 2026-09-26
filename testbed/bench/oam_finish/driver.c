/* oam_finish: the end of a busy frame commit. Last commit wrote 32 slots;
 * this one staged 29 (generation 9), 21 of them slots it shares with last
 * commit and 8 new ones, so 11 of last commit's slots vanished and are
 * parked off screen, and the 29 become the previous set. Marks of slots
 * staged in older commits (generation 8, 7) must not count as staged. The
 * shadow starts as a fixed pattern so the high-table read-modify-write
 * shows. */
#include "bench.h"
#include "loom_oam.h"

#if !defined(__65816__)
u8 oamMemory[544];
#endif

#define PREVIOUS 32

static const loom_u8 fresh[8] = {96, 97, 99, 101, 104, 106, 109, 110};

void bench_setup(void)
{
    loom_u16 i;

    for (i = 0; i < 544u; i++)
        oamMemory[i] = (u8)(i * 53u + 7u);
    for (i = 0; i < 128u; i++)
        loom_pvs_oam_mark[i] = (i & 1u) ? 0xffu : 7u;
    for (i = 0; i < 34u; i++) {
        loom_pvs_oam_current[i] = 0u;
        loom_pvs_oam_previous[i] = 0u;
    }
    loom_pvs_oam_generation = 9u;
    /* Last commit: slots 3, 8, 13, ... (5k + 3 mod 128, all distinct). */
    for (i = 0; i < PREVIOUS; i++) {
        loom_u8 slot = (loom_u8)((i * 5u + 3u) & 127u);
        loom_pvs_oam_previous[i] = slot;
        loom_pvs_oam_mark[slot] = 8u;
    }
    /* This commit: 21 of those (all but k % 8 in {2, 5} and k < 4), in the
     * reverse order, plus 8 slots last commit did not use. */
    loom_pvs_oam_count = 0u;
    for (i = PREVIOUS; i != 0u; i--) {
        loom_u8 k = (loom_u8)(i - 1u);
        loom_u8 slot = loom_pvs_oam_previous[k];
        if ((k & 7u) == 2u || (k & 7u) == 5u || k < 4u)
            continue;
        loom_pvs_oam_current[loom_pvs_oam_count++] = slot;
        loom_pvs_oam_mark[slot] = 9u;
    }
    for (i = 0; i < 8u; i++) {
        loom_u8 slot = fresh[i];
        loom_pvs_oam_current[loom_pvs_oam_count++] = slot;
        loom_pvs_oam_mark[slot] = 9u;
    }
    loom_pvs_oam_previous_count = PREVIOUS;
}

void bench_run(void)
{
    loom_pvs_oam_finish();
}

static loom_u16 fold(const u8 *bytes, loom_u16 count)
{
    loom_u16 h = 0x1234u;
    loom_u16 i;

    for (i = 0; i < count; i++)
        h = (loom_u16)((loom_u16)(h << 3) ^ (loom_u16)(h >> 13) ^ bytes[i]);
    return h;
}

void bench_check(void)
{
    BENCH_OUT(loom_pvs_oam_count);
    BENCH_OUT(loom_pvs_oam_previous_count);
    BENCH_OUT(loom_pvs_oam_generation);
    BENCH_OUT(fold(loom_pvs_oam_previous, 34u));
    BENCH_OUT(fold(loom_pvs_oam_current, 34u));
    BENCH_OUT(fold(loom_pvs_oam_mark, 128u));
    BENCH_OUT(fold(oamMemory, 128u));
    BENCH_OUT(fold(oamMemory + 128, 128u));
    BENCH_OUT(fold(oamMemory + 256, 128u));
    BENCH_OUT(fold(oamMemory + 384, 128u));
    BENCH_OUT(fold(oamMemory + 512, 32u));
    /* Slot 3 (k = 0, vanished), slot 23 (k = 4, kept), and the high-table
     * bytes of slots 0-3 and 20-23. */
    BENCH_OUT((loom_u16)(oamMemory[12] | (oamMemory[13] << 8)));
    BENCH_OUT((loom_u16)(oamMemory[92] | (oamMemory[93] << 8)));
    BENCH_OUT((loom_u16)(oamMemory[512] | (oamMemory[517] << 8)));
}
