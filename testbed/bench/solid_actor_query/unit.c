/* The C that body.asm's solid-actor queries replaced (Loom ea4e3af): the
 * assembly bodies called movement.c's shims, which went through the
 * generated hooks to the pool's queries. Verbatim from ea4e3af^:
 * runtime/src/movement.c:48-60 (the shims), the generated hooks
 * (crates/loom-app/src/generate/output.rs:6128-6131, a project with placed
 * actors), runtime/src/actor.c:1480-1534 and 1560-1604 (loom_actor_floor_below,
 * loom_actor_blocks_box) and runtime/src/mode1.c:1663-1666
 * (loom_mode1_visibility_table, over a cut-down Mode 1 state holding only
 * the visibility table). loom_actor_resolving is actor.c's static, false
 * outside the C resolve, which is when the assembly bodies ask. */
#include "loom_actor.h"

static loom_u8 loom_actor_resolving;

const loom_u8 *loom_mode1_visibility_table(void)
{
    return loom_mode1_sprite_visible;
}

loom_u8 loom_actor_floor_below(loom_s16 left,
                               loom_s16 right,
                               loom_s16 top,
                               loom_s16 bottom,
                               loom_u8 self,
                               loom_s16 *floor)
{
    const loom_u8 *visible;
    loom_u8 index;
    loom_u8 position;
    loom_u8 count;
    loom_u8 found;
    loom_s16 best;

    found = LOOM_ACTOR_INVALID_INDEX;
    best = 0;
    if (loom_actor_pool.solid_count == 0u ||
        loom_actor_pool.initialized == LOOM_FALSE) {
        return found;
    }
    visible = loom_mode1_visibility_table();
    count = loom_actor_pool.solid_count;
    for (position = 0u; position < count; ++position) {
        const LoomActorType *type;
        loom_s16 actor_left;
        loom_s16 actor_top;

        index = loom_actor_pool.solid_slots[position];
        if (index == self) {
            continue;
        }
        type = loom_actor_pool.type_ptr[index];
        if (loom_actor_pool.sprite_index[index] == 0xffu ||
            visible[loom_actor_pool.sprite_index[index]] == LOOM_FALSE) {
            continue;
        }
        actor_left = (loom_s16)(loom_actor_pool.x[index] + type->box_x);
        if (actor_left > right ||
            (loom_s16)(actor_left + (loom_s16)type->box_width - 1) < left) {
            continue;
        }
        actor_top = (loom_s16)(loom_actor_pool.y[index] + type->box_y);
        if (actor_top < top || actor_top > bottom) {
            continue;
        }
        if (found == LOOM_ACTOR_INVALID_INDEX || actor_top < best) {
            found = index;
            best = actor_top;
        }
    }
    if (found != LOOM_ACTOR_INVALID_INDEX) {
        *floor = (loom_s16)(best - 1);
    }
    return found;
}

loom_u8 loom_actor_blocks_box(loom_s16 left,
                              loom_s16 top,
                              loom_s16 right,
                              loom_s16 bottom)
{
    const loom_u8 *visible;
    loom_u8 index;
    loom_u8 position;
    loom_u8 count;

    if (loom_actor_pool.solid_count == 0u ||
        loom_actor_resolving != LOOM_FALSE ||
        loom_actor_pool.initialized == LOOM_FALSE) {
        return LOOM_FALSE;
    }
    /* One index over the pool's arrays: the walking-pointer form of this
     * sweep cost more than the accessor calls it replaced (#150). */
    visible = loom_mode1_visibility_table();
    count = loom_actor_pool.solid_count;
    for (position = 0u; position < count; ++position) {
        const LoomActorType *type;
        loom_s16 actor_left;
        loom_s16 actor_top;

        index = loom_actor_pool.solid_slots[position];
        type = loom_actor_pool.type_ptr[index];
        /* A gated actor the scene has hidden is not there to be walked
         * into; a slot the scene never mapped is not there at all. */
        if (loom_actor_pool.sprite_index[index] == 0xffu ||
            visible[loom_actor_pool.sprite_index[index]] == LOOM_FALSE) {
            continue;
        }
        actor_left = (loom_s16)(loom_actor_pool.x[index] + type->box_x);
        if (left > (loom_s16)(actor_left + (loom_s16)type->box_width - 1) ||
            right < actor_left) {
            continue;
        }
        actor_top = (loom_s16)(loom_actor_pool.y[index] + type->box_y);
        if (top <= (loom_s16)(actor_top + (loom_s16)type->box_height - 1) &&
            bottom >= actor_top) {
            return LOOM_TRUE;
        }
    }
    return LOOM_FALSE;
}

loom_u8 loom_generated_actor_blocks_box(loom_s16 left,
    loom_s16 top,
    loom_s16 right,
    loom_s16 bottom)
{
    return loom_actor_blocks_box(left, top, right, bottom);
}

loom_u8 loom_generated_actor_floor_below(loom_s16 left,
    loom_s16 right,
    loom_s16 top,
    loom_s16 bottom,
    loom_s16 *floor)
{
    return loom_actor_floor_below(left, right, top, bottom, LOOM_ACTOR_INVALID_INDEX, floor);
}

loom_u16 loom_movement_actor_floor_probe(loom_s16 left, loom_s16 right,
                                         loom_s16 top, loom_s16 bottom)
{
    return loom_generated_actor_floor_below(left, right, top, bottom,
                                            &loom_movement_probe_floor);
}

loom_u16 loom_movement_actor_block_probe(loom_s16 left, loom_s16 top,
                                         loom_s16 right, loom_s16 bottom)
{
    return loom_generated_actor_blocks_box(left, top, right, bottom);
}
