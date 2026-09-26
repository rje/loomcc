/* The C that body.asm's player sprite placement replaced (Loom 5184be9^:
 * runtime/src/mode1.c:1416-1464, loom_mode1_set_sprite_position and
 * loom_mode1_sprite_index, which loom_movement_update called after the
 * assembly player tick), as a function with the benchmark's interface
 * (unit.asm's loom_pvs_player_sprite). The assembly returns no status and
 * checks neither the initialized flag nor the slot range, so the status
 * returns are plain returns here; the initialized/scene check and the slot
 * range check stay, as the C had them. loom_pvs_mode1_bind (the assembly's
 * binding) has nothing to do in C. */
#include "loom_types.h"

void loom_pvs_mode1_bind(loom_s16 *world_x, loom_s16 *world_y,
                         loom_u8 *part_count, loom_u16 sprite_count,
                         const loom_u8 *slot_index, loom_s16 *camera_xy,
                         loom_s16 camera_min_x, loom_s16 camera_min_y,
                         loom_s16 camera_max_x, loom_s16 camera_max_y)
{
    (void)world_x;
    (void)world_y;
    (void)part_count;
    (void)sprite_count;
    (void)slot_index;
    (void)camera_xy;
    (void)camera_min_x;
    (void)camera_min_y;
    (void)camera_max_x;
    (void)camera_max_y;
}

static loom_s16 loom_mode1_sprite_index(loom_u8 slot)
{
    loom_u8 index;

    if (slot > LOOM_OAM_SLOT_MAX) {
        return (loom_s16)-1;
    }
    index = loom_mode1_state.slot_index[slot];
    if (index == 0xffu) {
        return (loom_s16)-1;
    }
    return (loom_s16)index;
}

void loom_pvs_player_sprite(loom_u8 slot, loom_s16 world_x, loom_s16 world_y)
{
    loom_s16 index;

    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return;
    }
    index = loom_mode1_sprite_index(slot);
    if (index < 0) {
        return;
    }
    {
        /* The parts of a metasprite follow the slot they belong to. Each
         * keeps its own pivot, so the same world position draws them in
         * their authored places around it. */
        loom_u8 remaining;
        loom_u8 cursor;

        cursor = (loom_u8)index;
        remaining = loom_mode1_state.sprite_part_count[cursor];
        loom_mode1_state.sprite_world_x[cursor] = world_x;
        loom_mode1_state.sprite_world_y[cursor] = world_y;
        while (remaining != 0u &&
               (loom_u16)(cursor + 1u) < loom_mode1_state.scene->sprite_count) {
            ++cursor;
            --remaining;
            loom_mode1_state.sprite_world_x[cursor] = world_x;
            loom_mode1_state.sprite_world_y[cursor] = world_y;
        }
    }
}
