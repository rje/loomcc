/* The C that runtime/backends/pvsneslib/src/oam.asm:loom_pvs_oam_finish
 * replaced (Loom 2e278a7), cut out as a function with the assembly
 * routine's name, interface and RAM: park every slot written last commit
 * that this commit did not stage, then remember this commit's slots.
 *
 * 2e278a7^ cleared the whole OAM with pvsneslib's oamClear every commit;
 * that is not isolatable (and was replaced, not transliterated). The C here
 * is the portable rendition 2e278a7 kept for the host:
 * runtime/backends/pvsneslib/src/runtime-adapter.c:953-973 at 2e278a7 (the
 * hide loop of loom_pvs_target_prepare_oam) verbatim except that "staged
 * this commit" is the assembly's test, mark[slot] == generation, instead of
 * the host's separate TRUE/FALSE mark; and the previous-slot bookkeeping
 * (the host wrote *previous = slot while writing each entry, and
 * previous_count = entry_count) done from loom_pvs_oam_current as the
 * assembly does. */
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

void loom_pvs_oam_finish(void)
{
    const loom_u8 *current;
    loom_u8 *previous;
    loom_u8 remaining;

    previous = loom_pvs_oam_previous;
    for (remaining = loom_pvs_oam_previous_count; remaining != 0u;
         --remaining, ++previous) {
        loom_u8 slot;

        slot = *previous;
        if (loom_pvs_oam_mark[slot] != loom_pvs_oam_generation) {
            /* Park the sprite at x = -256, y = 240: off screen at any size. */
            u8 *low;
            u8 *high;
            u8 quarter;

            low = &oamMemory[(u16)slot << 2];
            low[0] = 0u;
            low[1] = 240u;
            high = &oamMemory[512u + (slot >> 2)];
            quarter = (u8)(slot & 3u);
            *high = (u8)((*high & loom_pvs_runtime_oam_high_keep[quarter]) |
                         loom_pvs_runtime_oam_high_x[quarter]);
        }
    }
    current = loom_pvs_oam_current;
    previous = loom_pvs_oam_previous;
    for (remaining = loom_pvs_oam_count; remaining != 0u;
         --remaining, ++current, ++previous) {
        *previous = *current;
    }
    loom_pvs_oam_previous_count = loom_pvs_oam_count;
    loom_pvs_oam_count = 0u;
}
