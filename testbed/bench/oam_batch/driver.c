/* oam_batch: the Mode 1 sprite build for a 10-sprite scene (player, actors,
 * pickups, a large boss) with the camera at (180, 90), built into the OAM
 * shadow under generation 2, then a two-sprite batch (two more sprites)
 * whose second sprite reuses a slot staged earlier in the commit (rejected,
 * status 3). The scene covers a hidden sprite, sprites culled on the left,
 * right and bottom of the view, sprites partly off the left and top edges
 * (the high-table x bit, negative y), flips, large sprites and tile indices
 * past 255. Slots still marked with the last generation (1) are not
 * duplicates. */
#include "bench.h"
#include "loom_oam.h"

#if !defined(__65816__)
u8 oamMemory[544];
#endif

#define SPRITES 12

static LoomMode1Sprite sprites[SPRITES];
static LoomMode1SpritePose poses[SPRITES];
static loom_u8 visible[SPRITES];
static loom_s16 world_x[SPRITES];
static loom_s16 world_y[SPRITES];
static LoomMode1SpriteBatch batch_b;
static LoomMode1SpriteBatch batch_c;
static LoomStatus results[2];

static void sprite_at(loom_u8 i, loom_s16 x, loom_s16 y, loom_u8 shown,
                      loom_u8 slot, loom_u8 priority, loom_u8 flags,
                      loom_s16 pivot_x, loom_s16 pivot_y, loom_u16 tile,
                      loom_u8 palette, loom_u8 size, loom_u8 w, loom_u8 h)
{
    world_x[i] = x;
    world_y[i] = y;
    visible[i] = shown;
    sprites[i].world_x = x;
    sprites[i].world_y = y;
    sprites[i].pivot_x = pivot_x;
    sprites[i].pivot_y = pivot_y;
    sprites[i].tile_index = tile;
    sprites[i].slot = slot;
    sprites[i].palette = palette;
    sprites[i].priority = priority;
    sprites[i].size = size;
    sprites[i].flags = flags;
    sprites[i].width = w;
    sprites[i].height = h;
    poses[i].pivot_x = pivot_x;
    poses[i].pivot_y = pivot_y;
    poses[i].tile_index = tile;
    poses[i].palette = palette;
    poses[i].size = size;
    poses[i].width = w;
    poses[i].height = h;
    poses[i].reserved = 0u;
}

static void batch_of(LoomMode1SpriteBatch *batch, loom_u8 first, loom_u8 count,
                     loom_s16 camera_x, loom_s16 camera_y)
{
    batch->sprites = &sprites[first];
    batch->poses = &poses[first];
    batch->visible = &visible[first];
    batch->world_x = &world_x[first];
    batch->world_y = &world_y[first];
    batch->camera_x = camera_x;
    batch->camera_y = camera_y;
    batch->count = count;
    batch->reserved = 0u;
}

void bench_setup(void)
{
    loom_u16 i;

    for (i = 0; i < 544u; i++)
        oamMemory[i] = (u8)(i * 29u + 5u);
    for (i = 0; i < 128u; i++)
        loom_pvs_oam_mark[i] = 0xffu;
    for (i = 0; i < 34u; i++) {
        loom_pvs_oam_current[i] = 0u;
        loom_pvs_oam_previous[i] = 0u;
    }
    loom_pvs_oam_generation = 2u;
    loom_pvs_oam_count = 0u;
    loom_pvs_oam_previous_count = 0u;
    loom_pvs_oam_mark[0] = 1u;
    loom_pvs_oam_mark[3] = 1u;
    loom_pvs_oam_mark[6] = 1u;

    /* Staged slots are numbered in staging order (see the notes: oam.asm
     * at 7b4d960 indexes loom_pvs_oam_current by slot, not by count).
     *         i   x    y   vis slot pri fl  pvx pvy tile  pal sz  w   h */
    sprite_at(0, 220, 150, 1, 0, 2, 0, 8, 16, 0x000, 0, 1, 16, 32);  /* player */
    sprite_at(1, 190, 100, 1, 1, 2, 1, 8, 8, 0x020, 1, 0, 16, 16);   /* at the corner */
    sprite_at(2, 360, 200, 1, 2, 3, 3, 16, 16, 0x140, 3, 1, 32, 32); /* large, flipped */
    sprite_at(3, 250, 100, 0, 30, 2, 0, 8, 8, 0x02c, 1, 0, 16, 16);  /* hidden */
    sprite_at(4, 190, 200, 1, 3, 2, 2, 32, 48, 0x1c0, 6, 1, 64, 64); /* boss, off left */
    sprite_at(5, 500, 150, 1, 31, 2, 0, 8, 8, 0x030, 1, 0, 16, 16);  /* culled right */
    sprite_at(6, 330, 250, 1, 4, 0, 0, 4, 4, 0x1f8, 7, 0, 8, 8);     /* pickup */
    sprite_at(7, 105, 60, 1, 33, 2, 1, 8, 8, 0x024, 2, 0, 16, 16);   /* culled left */
    sprite_at(8, 300, 95, 1, 5, 1, 2, 8, 8, 0x028, 5, 0, 16, 16);    /* off top */
    sprite_at(9, 240, 400, 1, 32, 1, 1, 8, 8, 0x034, 4, 0, 16, 16);  /* culled below */
    sprite_at(10, 250, 150, 1, 6, 3, 3, 4, 4, 0x1fa, 7, 0, 8, 8);    /* second batch */
    sprite_at(11, 260, 160, 1, 0, 3, 0, 4, 4, 0x1fc, 7, 0, 8, 8);    /* slot 0, the player's */

    batch_of(&batch_b, 0, 10, 180, 90);
    batch_of(&batch_c, 10, 2, 180, 90);
}

void bench_run(void)
{
    results[0] = loom_pvs_oam_batch(&batch_b);
    results[1] = loom_pvs_oam_batch(&batch_c);
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

    BENCH_OUT(results[0]);
    BENCH_OUT(results[1]);
    BENCH_OUT(loom_pvs_oam_count);
    BENCH_OUT(loom_pvs_oam_generation);
    BENCH_OUT(fold(loom_pvs_oam_current, 34u));
    BENCH_OUT(fold(loom_pvs_oam_mark, 128u));
    BENCH_OUT(fold(oamMemory, 128u));
    BENCH_OUT(fold(oamMemory + 128, 128u));
    BENCH_OUT(fold(oamMemory + 256, 128u));
    BENCH_OUT(fold(oamMemory + 384, 128u));
    BENCH_OUT(fold(oamMemory + 512, 32u));
    /* Slots 0..9 verbatim (x/y and tile/attributes words). */
    for (i = 0; i < 10u; i++) {
        BENCH_OUT((loom_u16)(oamMemory[i * 4u] | (oamMemory[i * 4u + 1u] << 8)));
        BENCH_OUT((loom_u16)(oamMemory[i * 4u + 2u] | (oamMemory[i * 4u + 3u] << 8)));
    }
}
