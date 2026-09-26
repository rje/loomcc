/* The C that runtime/backends/pvsneslib/src/oam.asm:loom_pvs_oam_stage
 * replaced (Loom 2e278a7), cut out as a function with the assembly
 * routine's name, interface and RAM.
 *
 * 2e278a7^ staged entries into an array, sorted it, and at commit called
 * oamClear plus pvsneslib's oamSet/oamSetEx per entry; that path is not
 * isolatable (and was rewritten, not transliterated). The C here is the
 * portable rendition 2e278a7 kept for the host, which implements the same
 * contract the assembly does:
 *   - validation and the duplicate-slot generation mark:
 *     runtime/backends/pvsneslib/src/frame-transaction.c:134-158 at 2e278a7
 *     (loom_port_oam_stage, the non-65816 branch);
 *   - the shadow write: runtime/backends/pvsneslib/src/runtime-adapter.c:
 *     974-1012 at 2e278a7 (the body of loom_pvs_target_prepare_oam's last
 *     loop), done at stage time as the assembly does, with the slot
 *     recorded in loom_pvs_oam_current as oam.asm does. */
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

LoomStatus loom_pvs_oam_stage(const LoomOamEntry *entry)
{
    loom_u8 slot;
    u8 *low;
    u8 *high;
    u8 quarter;
    u8 bits;

    /* One OR of every out-of-range bit and one unsigned window test per
     * coordinate: each separate comparison costs 816-tcc a pointer reload
     * and a signed compare sequence, and this runs once per sprite. */
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

    /* Write the OAM shadow directly: pvsneslib's oamSet and oamSetEx
     * cost several scanlines per sprite. */
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
