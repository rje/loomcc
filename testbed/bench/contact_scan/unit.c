/* The C that body.asm's loom_pvs_combat_touch replaced (Loom 925fc3a^:
 * runtime/src/combat.c:120-140, loom_combat_actor_touches_player, and
 * :447-469, the head of loom_combat_update's contact loop up to the shot
 * check), cut out as a function with the assembly routine's name and
 * interface: scan from `start` for the first live, awake actor with a hit box
 * and contact damage whose hit box covers the player's box, other than the
 * player's own shot type; its index, or 0xffff. The loop body after the shot
 * check (settling the touch) stayed in C. */
#include "loom_actor.h"

/* True when the actor's hit box covers the player's body box. */
static loom_u8 loom_combat_actor_touches_player(const LoomActorType *type,
                                                loom_s16 actor_x,
                                                loom_s16 actor_y,
                                                loom_s16 player_left,
                                                loom_s16 player_top,
                                                loom_s16 player_right,
                                                loom_s16 player_bottom)
{
    loom_s16 left;
    loom_s16 top;
    loom_s16 right;
    loom_s16 bottom;

    left = (loom_s16)(actor_x + type->hit_x);
    top = (loom_s16)(actor_y + type->hit_y);
    right = (loom_s16)(left + (loom_s16)type->hit_width - 1);
    bottom = (loom_s16)(top + (loom_s16)type->hit_height - 1);
    return (loom_u8)(player_left <= right && player_right >= left &&
                     player_top <= bottom && player_bottom >= top);
}

loom_u16 loom_pvs_combat_touch(loom_u16 start, loom_u16 count,
                               loom_s16 player_left, loom_s16 player_top,
                               loom_s16 player_right, loom_s16 player_bottom,
                               const LoomActorType *shot_type)
{
    loom_u8 index;

    for (index = (loom_u8)start; index < count; ++index) {
        const LoomActorType *type;

        /* A sleeping actor is off the screen; it cannot touch the player. */
        if (loom_actor_pool.alive[index] == LOOM_FALSE ||
            loom_actor_pool.awake[index] == LOOM_FALSE) {
            continue;
        }
        type = loom_actor_pool.type_ptr[index];
        if (type->contact_damage == 0u || type->hit_width == 0u ||
            type->hit_height == 0u) {
            continue;
        }
        if (loom_combat_actor_touches_player(
                type, loom_actor_pool.x[index], loom_actor_pool.y[index],
                player_left, player_top, player_right,
                player_bottom) == LOOM_FALSE) {
            continue;
        }
        /* The player's own shot, tested only on the rare overlap. */
        if (type == shot_type) {
            continue;
        }
        return (loom_u16)index;
    }
    return 0xffffu;
}
