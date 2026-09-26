/* The C that runtime/backends/pvsneslib/src/oam.asm:loom_pvs_oam_batch
 * replaced (Loom 7b4d960), cut out as a function with the assembly
 * routine's name, interface and RAM. At 7b4d960^ a sprite went through:
 *   1. runtime/src/mode1.c:526-580 (loom_mode1_add_sprites): compute the
 *      screen position, cull against the view, reserve a frame-build OAM
 *      entry and fill it -- here verbatim, reading the batch descriptor as
 *      the host rendition kept at 7b4d960 does (mode1.c:548-598);
 *   2. runtime/src/frame-build.c:143-154 (loom_frame_build_reserve_oam);
 *   3. runtime/src/frame-build.c:202-209 at submit: loom_port_oam_stage per
 *      reserved entry, which on the console called oam.asm's
 *      loom_pvs_oam_stage. That stage is given here as the host C rendition
 *      of the same contract (frame-transaction.c:134-158 validation and slot
 *      mark, runtime-adapter.c:974-1012 shadow write, at 2e278a7), so the C
 *      side is all C. */
#include "loom_oam.h"

loom_u8 loom_pvs_oam_generation;
loom_u8 loom_pvs_oam_count;
loom_u8 loom_pvs_oam_previous_count;
loom_u8 loom_pvs_oam_mark[128];
loom_u8 loom_pvs_oam_current[34];
loom_u8 loom_pvs_oam_previous[34];

/* OAM high-table bit positions for slot & 3; variable shifts are loops on
 * 816-tcc, a table lookup is one indexed load. */
static const u8 loom_pvs_runtime_oam_high_keep[4] = {0xfcu, 0xf3u, 0xcfu,
                                                     0x3fu};
static const u8 loom_pvs_runtime_oam_high_x[4] = {0x01u, 0x04u, 0x10u, 0x40u};
static const u8 loom_pvs_runtime_oam_high_large[4] = {0x02u, 0x08u, 0x20u,
                                                      0x80u};

static LoomOamEntry loom_frame_build_oam[LOOM_FRAME_OAM_CAPACITY];
static loom_u8 loom_frame_build_oam_count;

static LoomOamEntry *loom_frame_build_reserve_oam(void)
{
    LoomOamEntry *entry;

    if (loom_frame_build_oam_count >= LOOM_FRAME_OAM_CAPACITY) {
        return (LoomOamEntry *)0;
    }
    entry = &loom_frame_build_oam[loom_frame_build_oam_count];
    ++loom_frame_build_oam_count;
    return entry;
}

static LoomStatus loom_oam_stage(const LoomOamEntry *entry)
{
    loom_u8 slot;
    u8 *low;
    u8 *high;
    u8 quarter;
    u8 bits;

    slot = entry->slot;
    if (((loom_u8)(slot & (loom_u8)(~LOOM_OAM_SLOT_MAX)) |
         (loom_u8)(entry->palette & (loom_u8)(~7u)) |
         (loom_u8)(entry->priority & (loom_u8)(~3u)) |
         (loom_u8)(entry->size & (loom_u8)(~LOOM_OAM_SIZE_LARGE)) |
         (loom_u8)(entry->flags &
                   (loom_u8)(~(LOOM_OAM_FLAG_FLIP_X |
                               LOOM_OAM_FLAG_FLIP_Y))) |
         entry->reserved) != 0u ||
        entry->tile_index > LOOM_OAM_TILE_INDEX_MAX ||
        (loom_u16)(entry->x - LOOM_OAM_X_MIN) >
            (loom_u16)(LOOM_OAM_X_MAX - LOOM_OAM_X_MIN) ||
        (loom_u16)(entry->y - LOOM_OAM_Y_MIN) >
            (loom_u16)(LOOM_OAM_Y_MAX - LOOM_OAM_Y_MIN)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_pvs_oam_mark[slot] == loom_pvs_oam_generation) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_pvs_oam_mark[slot] = loom_pvs_oam_generation;
    loom_pvs_oam_current[loom_pvs_oam_count] = slot;
    ++loom_pvs_oam_count;

    low = &oamMemory[(u16)slot << 2];
    low[0] = (u8)entry->x;
    low[1] = (u8)entry->y;
    low[2] = (u8)entry->tile_index;
    bits = (u8)((entry->priority << 4) | (entry->palette << 1) |
                (u8)((entry->tile_index >> 8) & 1u));
    if ((entry->flags & LOOM_OAM_FLAG_FLIP_Y) != 0u) {
        bits |= 0x80u;
    }
    if ((entry->flags & LOOM_OAM_FLAG_FLIP_X) != 0u) {
        bits |= 0x40u;
    }
    low[3] = bits;
    high = &oamMemory[512u + (slot >> 2)];
    quarter = (u8)(slot & 3u);
    bits = (u8)(*high & loom_pvs_runtime_oam_high_keep[quarter]);
    if (entry->x < 0) {
        bits |= loom_pvs_runtime_oam_high_x[quarter];
    }
    if (entry->size == LOOM_OAM_SIZE_LARGE) {
        bits |= loom_pvs_runtime_oam_high_large[quarter];
    }
    *high = bits;
    return LOOM_STATUS_OK;
}

LoomStatus loom_pvs_oam_batch(const LoomMode1SpriteBatch *batch)
{
    const LoomMode1Sprite *sprite;
    const LoomMode1SpritePose *pose;
    const loom_u8 *visible;
    const loom_s16 *world_x;
    const loom_s16 *world_y;
    loom_u8 remaining;
    loom_u8 index;

    /* The frame build opens empty each frame. */
    loom_frame_build_oam_count = 0u;

    sprite = batch->sprites;
    pose = batch->poses;
    visible = batch->visible;
    world_x = batch->world_x;
    world_y = batch->world_y;
    for (remaining = batch->count; remaining != 0u;
         --remaining, ++sprite, ++pose, ++visible, ++world_x,
         ++world_y) {
        LoomOamEntry *entry;
        loom_s16 screen_x;
        loom_s16 screen_y;

        if (*visible == LOOM_FALSE) {
            continue;
        }
        screen_x = (loom_s16)(*world_x - batch->camera_x -
                              pose->pivot_x);
        if (screen_x <= (loom_s16)(0 - (loom_s16)pose->width) ||
            screen_x >= (loom_s16)LOOM_MODE1_VIEW_WIDTH) {
            continue;
        }
        screen_y = (loom_s16)(*world_y - batch->camera_y -
                              pose->pivot_y);
        if (screen_y <= (loom_s16)(0 - (loom_s16)pose->height) ||
            screen_y >= (loom_s16)LOOM_MODE1_VIEW_HEIGHT) {
            continue;
        }
        entry = loom_frame_build_reserve_oam();
        if (entry == (LoomOamEntry *)0) {
            return LOOM_STATUS_CAPACITY;
        }
        entry->x = screen_x;
        entry->y = screen_y;
        entry->tile_index = pose->tile_index;
        entry->slot = sprite->slot;
        entry->palette = pose->palette;
        entry->priority = sprite->priority;
        entry->size = pose->size;
        entry->flags = sprite->flags;
        entry->reserved = 0u;
    }

    /* Submit: stage every reserved entry. */
    for (index = 0u; index < loom_frame_build_oam_count; ++index) {
        LoomStatus status;

        status = loom_oam_stage(&loom_frame_build_oam[index]);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    return LOOM_STATUS_OK;
}
