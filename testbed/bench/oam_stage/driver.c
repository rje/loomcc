/* oam_stage: one frame commit's sprites staged into the OAM shadow. The
 * shadow starts with last frame's contents (a fixed pattern, so the
 * high-table read-modify-write shows), the commit's generation is 5, two
 * slots still carry the previous generation's mark and one slot was already
 * staged this commit. Twenty entries: eleven valid ones (on screen, partly
 * off the left edge, above the top, flipped, large, tile index past 255,
 * every slot quarter) and nine rejected ones (a duplicate slot, a slot
 * staged earlier this commit, slot 128, tile 512, x -257, y 256, palette 8, size 2, a reserved byte). */
#include "bench.h"
#include "loom_oam.h"

#if !defined(__65816__)
u8 oamMemory[544];
#endif

#define ENTRIES 20

static LoomOamEntry entries[ENTRIES];
static LoomStatus results[ENTRIES];

static void entry_at(loom_u8 i, loom_s16 x, loom_s16 y, loom_u16 tile,
                     loom_u8 slot, loom_u8 palette, loom_u8 priority,
                     loom_u8 size, loom_u8 flags, loom_u8 reserved)
{
    entries[i].x = x;
    entries[i].y = y;
    entries[i].tile_index = tile;
    entries[i].slot = slot;
    entries[i].palette = palette;
    entries[i].priority = priority;
    entries[i].size = size;
    entries[i].flags = flags;
    entries[i].reserved = reserved;
}

void bench_setup(void)
{
    loom_u16 i;

    for (i = 0; i < 544u; i++)
        oamMemory[i] = (u8)(i * 37u + 11u);
    for (i = 0; i < 128u; i++)
        loom_pvs_oam_mark[i] = 0xffu;
    for (i = 0; i < 34u; i++) {
        loom_pvs_oam_current[i] = 0u;
        loom_pvs_oam_previous[i] = 0u;
    }
    loom_pvs_oam_generation = 5u;
    loom_pvs_oam_count = 0u;
    loom_pvs_oam_previous_count = 0u;
    loom_pvs_oam_mark[0] = 4u;  /* staged last commit */
    loom_pvs_oam_mark[9] = 4u;
    loom_pvs_oam_mark[40] = 5u; /* already staged this commit */

    /*        i   x     y    tile slot pal pri size flags res */
    entry_at(0, 120, 100, 0x010, 0, 0, 2, 0, 0, 0);
    entry_at(1, -12, 64, 0x022, 1, 1, 2, 1, 1, 0);
    entry_at(2, 200, -8, 0x130, 2, 7, 3, 0, 2, 0);
    entry_at(3, 255, 223, 0x1ff, 3, 3, 0, 1, 3, 0);
    entry_at(4, -256, -64, 0x000, 9, 2, 1, 0, 0, 0);
    entry_at(5, 16, 180, 0x044, 17, 4, 2, 1, 0, 0);
    entry_at(6, 64, 48, 0x046, 17, 4, 2, 1, 0, 0);   /* duplicate slot */
    entry_at(7, 80, 48, 0x048, 40, 4, 2, 0, 0, 0);   /* staged already */
    entry_at(8, 80, 48, 0x048, 128, 4, 2, 0, 0, 0);  /* slot 128 */
    entry_at(9, 80, 48, 0x200, 41, 4, 2, 0, 0, 0);   /* tile 512 */
    entry_at(10, -257, 48, 0x048, 42, 4, 2, 0, 0, 0); /* x too small */
    entry_at(11, 80, 256, 0x048, 43, 4, 2, 0, 0, 0);  /* y too large */
    entry_at(12, 80, 48, 0x048, 44, 8, 2, 0, 0, 0);   /* palette 8 */
    entry_at(13, 80, 48, 0x048, 45, 4, 2, 2, 0, 0);   /* size 2 */
    entry_at(14, 80, 48, 0x048, 46, 4, 2, 0, 0, 1);   /* reserved */
    entry_at(15, 96, 150, 0x0a0, 64, 5, 2, 0, 1, 0);
    entry_at(16, -1, 150, 0x0a2, 65, 5, 2, 1, 2, 0);
    entry_at(17, 128, 0, 0x101, 66, 6, 1, 0, 0, 0);
    entry_at(18, 32, 32, 0x0c0, 127, 0, 3, 1, 3, 0);
    entry_at(19, 250, 200, 0x0c2, 126, 1, 3, 0, 0, 0);
}

#define S(i) results[i] = loom_pvs_oam_stage(&entries[i])

void bench_run(void)
{
    S(0);
    S(1);
    S(2);
    S(3);
    S(4);
    S(5);
    S(6);
    S(7);
    S(8);
    S(9);
    S(10);
    S(11);
    S(12);
    S(13);
    S(14);
    S(15);
    S(16);
    S(17);
    S(18);
    S(19);
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
    loom_u8 i;

    for (i = 0; i < ENTRIES; i += 2)
        BENCH_OUT((loom_u16)(results[i] | (results[i + 1] << 8)));
    BENCH_OUT(loom_pvs_oam_count);
    BENCH_OUT(loom_pvs_oam_generation);
    BENCH_OUT(fold(loom_pvs_oam_current, 34u));
    BENCH_OUT(fold(loom_pvs_oam_mark, 128u));
    BENCH_OUT(fold(oamMemory, 128u));
    BENCH_OUT(fold(oamMemory + 128, 128u));
    BENCH_OUT(fold(oamMemory + 256, 128u));
    BENCH_OUT(fold(oamMemory + 384, 128u));
    BENCH_OUT(fold(oamMemory + 512, 32u));
    /* A few staged records verbatim. */
    BENCH_OUT((loom_u16)(oamMemory[4] | (oamMemory[5] << 8)));
    BENCH_OUT((loom_u16)(oamMemory[6] | (oamMemory[7] << 8)));
    BENCH_OUT((loom_u16)(oamMemory[12] | (oamMemory[13] << 8)));
    BENCH_OUT((loom_u16)(oamMemory[14] | (oamMemory[15] << 8)));
    BENCH_OUT((loom_u16)(oamMemory[512] | (oamMemory[528] << 8)));
    BENCH_OUT((loom_u16)(oamMemory[543] | (oamMemory[514] << 8)));
}
