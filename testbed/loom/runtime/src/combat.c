#include <loom/combat.h>
#include <loom/movement.h>

typedef struct LoomCombatState {
    loom_u16 player_health;
    loom_u16 damage_count;
    loom_u8 invulnerable_ticks;
    loom_u8 game_over;
    loom_u8 initialized;
    loom_u8 contact_index;
    loom_u16 projectile_hits;
    /* Ticks before the authored attack will fire again. */
    loom_u16 attack_cooldown;
    /* The same, for the second player's own shots. */
    loom_u16 second_attack_cooldown;
    /* The authored attack's projectile type, a player's own shot that no
     * contact pass counts against either player; null without an attack.
     * Resolved once here: the passes run every tick on the budget's edge. */
    const LoomActorType *shot_type;
    /* The second player's own window after an enemy touches it. */
    loom_u8 second_invulnerable_ticks;
    /* Stompable actors killed from above since the last restart. */
    loom_u16 stomp_count;
} LoomCombatState;

static LoomCombatState loom_combat_state;

loom_u8 loom_combat_debug_epoch;

LoomStatus loom_combat_initialize(void)
{
    loom_combat_state.player_health = loom_generated_combat_player_max_health;
    loom_combat_state.damage_count = 0u;
    loom_combat_state.invulnerable_ticks = 0u;
    loom_combat_state.game_over = LOOM_FALSE;
    loom_combat_state.projectile_hits = 0u;
    loom_combat_state.attack_cooldown = 0u;
    loom_combat_state.second_attack_cooldown = 0u;
    loom_combat_state.shot_type =
        loom_generated_attack_enabled != LOOM_FALSE
            ? &loom_generated_actor_types[loom_generated_attack_type_index]
            : (const LoomActorType *)0;
    loom_combat_state.second_invulnerable_ticks = 0u;
    loom_combat_state.stomp_count = 0u;
    loom_combat_state.contact_index = LOOM_ACTOR_INVALID_INDEX;
    loom_combat_state.initialized = LOOM_TRUE;
    ++loom_combat_debug_epoch;
    return LOOM_STATUS_OK;
}

loom_u16 loom_combat_player_health(void)
{
    return loom_combat_state.player_health;
}

loom_u8 loom_combat_player_invulnerable(void)
{
    return loom_combat_state.invulnerable_ticks;
}

loom_u8 loom_combat_game_over(void)
{
    return loom_combat_state.game_over;
}

loom_u16 loom_combat_player_damage_count(void)
{
    return loom_combat_state.damage_count;
}

LoomStatus loom_combat_set_player_health(loom_u16 health)
{
    if (loom_combat_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (health > loom_generated_combat_player_max_health) {
        health = loom_generated_combat_player_max_health;
    }
    loom_combat_state.player_health = health;
    if (health == 0u) {
        loom_combat_state.game_over = LOOM_TRUE;
    }
    ++loom_combat_debug_epoch;
    return LOOM_STATUS_OK;
}

/* Pushes the player one whole step away from the actor that hit it, on the
 * axis the two are furthest apart. Movement validates the destination, so a
 * push into a wall simply does not happen. */
static void loom_combat_knock_back(loom_s16 actor_x,
                                   loom_s16 actor_y,
                                   loom_u8 distance)
{
    loom_s16 player_x;
    loom_s16 player_y;
    loom_s16 delta_x;
    loom_s16 delta_y;
    loom_u16 reach_x;
    loom_u16 reach_y;

    if (distance == 0u) {
        return;
    }
    player_x = loom_movement_player_x();
    player_y = loom_movement_player_y();
    delta_x = (loom_s16)(player_x - actor_x);
    delta_y = (loom_s16)(player_y - actor_y);
    reach_x = (loom_u16)(delta_x < 0 ? -delta_x : delta_x);
    reach_y = (loom_u16)(delta_y < 0 ? -delta_y : delta_y);
    if (reach_x >= reach_y) {
        player_x = (loom_s16)(player_x + (delta_x < 0 ? -(loom_s16)distance
                                                      : (loom_s16)distance));
    } else {
        player_y = (loom_s16)(player_y + (delta_y < 0 ? -(loom_s16)distance
                                                      : (loom_s16)distance));
    }
    (void)loom_movement_set_player_position(player_x, player_y);
}

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

/* A stomp: the player is falling and its feet are in the top half of the
 * hit box, so the actor dies instead of the player being hurt. A solid
 * actor is stood on, never stomped: the body stops above its box. */
static loom_u8 loom_combat_stomps(const LoomActorType *type,
                                  loom_s16 actor_y,
                                  loom_s16 player_bottom)
{
    loom_s16 top;

    if ((type->flags & LOOM_ACTOR_FLAG_STOMPABLE) == 0u ||
        loom_movement_velocity_y() <= 0) {
        return LOOM_FALSE;
    }
    top = (loom_s16)(actor_y + type->hit_y);
    return (loom_u8)(player_bottom <=
                     (loom_s16)(top + (loom_s16)(type->hit_height / 2u)));
}

/* Do two axis-aligned boxes, given as inclusive edges, share a pixel? */
static loom_u8 loom_combat_boxes_overlap(loom_s16 left,
                                         loom_s16 top,
                                         loom_s16 right,
                                         loom_s16 bottom,
                                         loom_s16 other_left,
                                         loom_s16 other_top,
                                         loom_s16 other_right,
                                         loom_s16 other_bottom)
{
    return (loom_u8)(left <= other_right && right >= other_left &&
                     top <= other_bottom && bottom >= other_top);
}

/* Every projectile in flight against every actor that can be hurt. Both sides
 * are small -- a declared pool of two or three, against actors carrying a
 * hurt box -- and a projectile is spent by the first thing it lands on, so
 * this is a short inner loop rather than a pairwise sweep of the pool. */
/* An enemy's hit box against a joined second player's hurt box: the
 * controller actor takes the contact damage through its own health, with
 * the enemy's window, so co-op hurts both players. A second player without
 * a hurt box or health cannot be hurt, as before. */
static void loom_combat_second_player(loom_u8 count)
{
    loom_u8 target;
    loom_u8 index;
    loom_s16 joined_x;
    loom_s16 joined_y;

    if (loom_combat_state.second_invulnerable_ticks != 0u) {
        --loom_combat_state.second_invulnerable_ticks;
        return;
    }
    /* One call, so solo play pays nothing for the pass. */
    if (loom_actor_controller_position(&joined_x, &joined_y) == LOOM_FALSE) {
        return;
    }
    for (target = 0u; target < count; ++target) {
        const LoomActorType *hurt;
        loom_s16 hurt_left;
        loom_s16 hurt_top;
        loom_s16 hurt_right;
        loom_s16 hurt_bottom;

        if (loom_actor_pool.alive[target] == LOOM_FALSE ||
            loom_actor_pool.health[target] == 0u ||
            loom_actor_pool.behavior_a[target] == 0u) {
            continue;
        }
        hurt = loom_actor_pool.type_ptr[target];
        if (hurt->behavior != LOOM_ACTOR_BEHAVIOR_CONTROLLER ||
            hurt->hurt_width == 0u || hurt->hurt_height == 0u) {
            continue;
        }
        hurt_left = (loom_s16)(loom_actor_pool.x[target] + hurt->hurt_x);
        hurt_top = (loom_s16)(loom_actor_pool.y[target] + hurt->hurt_y);
        hurt_right = (loom_s16)(hurt_left + (loom_s16)hurt->hurt_width - 1);
        hurt_bottom = (loom_s16)(hurt_top + (loom_s16)hurt->hurt_height - 1);
        for (index = 0u; index < count; ++index) {
            const LoomActorType *type;
            loom_s16 left;
            loom_s16 top;

            if (index == target || loom_actor_pool.alive[index] == LOOM_FALSE) {
                continue;
            }
            type = loom_actor_pool.type_ptr[index];
            if (type->contact_damage == 0u || type->hit_width == 0u ||
                type->hit_height == 0u ||
                type->behavior == LOOM_ACTOR_BEHAVIOR_CONTROLLER) {
                continue;
            }
            left = (loom_s16)(loom_actor_pool.x[index] + type->hit_x);
            top = (loom_s16)(loom_actor_pool.y[index] + type->hit_y);
            if (loom_combat_boxes_overlap(
                    left, top, (loom_s16)(left + (loom_s16)type->hit_width - 1),
                    (loom_s16)(top + (loom_s16)type->hit_height - 1), hurt_left,
                    hurt_top, hurt_right, hurt_bottom) == LOOM_FALSE) {
                continue;
            }
            /* A teammate's shot, tested only on the rare overlap. */
            if (type == loom_combat_state.shot_type) {
                continue;
            }
            (void)loom_actor_damage(target, type->contact_damage);
            loom_combat_state.second_invulnerable_ticks =
                type->invulnerable_ticks != 0u
                    ? type->invulnerable_ticks
                    : loom_generated_combat_player_invulnerable_ticks;
            ++loom_combat_debug_epoch;
            /* One hit per tick, as for player one. */
            return;
        }
    }
}

static void loom_combat_projectiles(loom_u8 count)
{
    loom_u8 index;

    for (index = 0u; index < count; ++index) {
        const LoomActorType *type;
        loom_s16 left;
        loom_s16 top;
        loom_s16 right;
        loom_s16 bottom;
        loom_u8 target;

        if (loom_actor_pool.alive[index] == LOOM_FALSE) {
            continue;
        }
        type = loom_actor_pool.type_ptr[index];
        if (type->behavior != LOOM_ACTOR_BEHAVIOR_PROJECTILE ||
            type->contact_damage == 0u || type->hit_width == 0u ||
            type->hit_height == 0u) {
            continue;
        }
        left = (loom_s16)(loom_actor_pool.x[index] + type->hit_x);
        top = (loom_s16)(loom_actor_pool.y[index] + type->hit_y);
        right = (loom_s16)(left + (loom_s16)type->hit_width - 1);
        bottom = (loom_s16)(top + (loom_s16)type->hit_height - 1);
        for (target = 0u; target < count; ++target) {
            const LoomActorType *hurt;
            loom_s16 hurt_left;
            loom_s16 hurt_top;

            if (target == index ||
                loom_actor_pool.alive[target] == LOOM_FALSE ||
                loom_actor_pool.awake[target] == LOOM_FALSE ||
                loom_actor_pool.health[target] == 0u) {
                continue;
            }
            hurt = loom_actor_pool.type_ptr[target];
            /* A second player is a teammate: neither player's shot lands
             * on it, as none lands on player one. */
            if (hurt->hurt_width == 0u || hurt->hurt_height == 0u ||
                hurt->behavior == LOOM_ACTOR_BEHAVIOR_PROJECTILE ||
                hurt->behavior == LOOM_ACTOR_BEHAVIOR_CONTROLLER) {
                continue;
            }
            hurt_left = (loom_s16)(loom_actor_pool.x[target] + hurt->hurt_x);
            hurt_top = (loom_s16)(loom_actor_pool.y[target] + hurt->hurt_y);
            if (loom_combat_boxes_overlap(
                    left, top, right, bottom, hurt_left, hurt_top,
                    (loom_s16)(hurt_left + (loom_s16)hurt->hurt_width - 1),
                    (loom_s16)(hurt_top + (loom_s16)hurt->hurt_height - 1)) ==
                LOOM_FALSE) {
                continue;
            }
            (void)loom_actor_damage(target, type->contact_damage);
            if (loom_combat_state.projectile_hits != 0xffffu) {
                ++loom_combat_state.projectile_hits;
            }
            /* A projectile is spent by what it hits, whether or not that
             * killed it. */
            (void)loom_actor_despawn(index);
            ++loom_combat_debug_epoch;
            break;
        }
    }
}

LoomStatus loom_combat_player_attack(const LoomInputSnapshot *input)
{
    loom_s8 facing_x;
    loom_s8 facing_y;
    loom_s16 launch;

    if (loom_combat_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_generated_attack_enabled == LOOM_FALSE ||
        loom_combat_state.game_over != LOOM_FALSE ||
        input == (const LoomInputSnapshot *)0 || input->pad_count == 0u) {
        return LOOM_STATUS_OK;
    }
    if (loom_combat_state.attack_cooldown != 0u) {
        --loom_combat_state.attack_cooldown;
        return LOOM_STATUS_OK;
    }
    if ((input->pads[0].pressed & loom_generated_attack_button) == 0u) {
        return LOOM_STATUS_OK;
    }
    facing_x = loom_movement_facing_x();
    facing_y = loom_movement_facing_y();
    if (facing_x == 0 && facing_y == 0) {
        /* A player that has not moved yet still has to be able to fire, and
         * down is where a top-down character starts out looking. */
        facing_y = 1;
    }
    launch = (loom_s16)loom_generated_attack_launch_px;
    (void)loom_actor_spawn(
        loom_generated_attack_type_index,
        (loom_s16)(loom_movement_player_x() + (loom_s16)(facing_x * launch)),
        (loom_s16)(loom_movement_player_y() + (loom_s16)(facing_y * launch)),
        facing_x, facing_y);
    loom_combat_state.attack_cooldown = loom_generated_attack_cooldown_ticks;
    ++loom_combat_debug_epoch;
    return LOOM_STATUS_OK;
}

LoomStatus loom_combat_second_player_attack(const LoomInputSnapshot *input)
{
    loom_s16 x;
    loom_s16 y;
    loom_s8 facing_x;
    loom_s8 facing_y;
    loom_u8 pad;
    loom_s16 launch;

    if (loom_combat_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_generated_attack_enabled == LOOM_FALSE ||
        loom_combat_state.game_over != LOOM_FALSE ||
        input == (const LoomInputSnapshot *)0) {
        return LOOM_STATUS_OK;
    }
    if (loom_combat_state.second_attack_cooldown != 0u) {
        --loom_combat_state.second_attack_cooldown;
        return LOOM_STATUS_OK;
    }
    /* One call, so solo play pays nothing for the second player's shot. */
    if (loom_actor_controller_aim(&x, &y, &facing_x, &facing_y, &pad) ==
        LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    if (pad >= input->pad_count ||
        (input->pads[pad].pressed & loom_generated_attack_button) == 0u) {
        return LOOM_STATUS_OK;
    }
    launch = (loom_s16)loom_generated_attack_launch_px;
    (void)loom_actor_spawn(loom_generated_attack_type_index,
                           (loom_s16)(x + (loom_s16)(facing_x * launch)),
                           (loom_s16)(y + (loom_s16)(facing_y * launch)),
                           facing_x, facing_y);
    loom_combat_state.second_attack_cooldown =
        loom_generated_attack_cooldown_ticks;
    ++loom_combat_debug_epoch;
    return LOOM_STATUS_OK;
}

/* An actor's hit box on the player: a stomp, a shielded touch, or damage.
 * True when the tick's combat is settled; false to keep scanning (the
 * player cannot be hurt and this actor was not stomped). */
static loom_u8 loom_combat_touched(loom_u8 index,
                                   const LoomActorType *type,
                                   loom_u8 shielded,
                                   loom_u8 hurtable,
                                   loom_s16 player_bottom)
{
    if (loom_combat_stomps(type, loom_actor_pool.y[index],
                           player_bottom) != LOOM_FALSE) {
        /* Landed on from above: the actor dies whatever its health,
         * the player bounces, and no damage changes hands. */
        if (loom_actor_pool.health[index] == 0u) {
            (void)loom_actor_despawn(index);
        } else {
            (void)loom_actor_damage(index, loom_actor_pool.health[index]);
        }
        (void)loom_movement_bounce();
        if (loom_combat_state.stomp_count != 0xffffu) {
            ++loom_combat_state.stomp_count;
        }
        ++loom_combat_debug_epoch;
        return LOOM_TRUE;
    }
    if (hurtable == LOOM_FALSE) {
        return LOOM_FALSE;
    }
    if (loom_combat_state.contact_index != index) {
        loom_combat_state.contact_index = index;
        ++loom_combat_debug_epoch;
    }
    if (shielded != LOOM_FALSE) {
        return LOOM_TRUE;
    }
    if (loom_combat_state.player_health > type->contact_damage) {
        loom_combat_state.player_health =
            (loom_u16)(loom_combat_state.player_health -
                       type->contact_damage);
    } else {
        loom_combat_state.player_health = 0u;
        loom_combat_state.game_over = LOOM_TRUE;
    }
    if (loom_combat_state.damage_count != 0xffffu) {
        ++loom_combat_state.damage_count;
    }
    loom_combat_state.invulnerable_ticks =
        type->invulnerable_ticks != 0u
            ? type->invulnerable_ticks
            : loom_generated_combat_player_invulnerable_ticks;
    loom_combat_knock_back(loom_actor_pool.x[index], loom_actor_pool.y[index],
                           type->knockback_px);
    ++loom_combat_debug_epoch;
    /* One hit per tick: two enemies touching at once still cost one. */
    return LOOM_TRUE;
}

LoomStatus loom_combat_update(void)
{
    loom_u8 count;
    loom_u8 index;
    loom_u8 shielded;
    loom_u8 hurtable;
    loom_s16 player_left;
    loom_s16 player_top;
    loom_s16 player_right;
    loom_s16 player_bottom;

    if (loom_combat_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_combat_state.game_over != LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    /* The window is read before it is spent, so it lasts exactly the ticks
     * the type asked for; the contact scan still runs, because a witness that
     * cannot see a withheld hit cannot explain one. */
    shielded = (loom_u8)(loom_combat_state.invulnerable_ticks != 0u);
    if (shielded != LOOM_FALSE) {
        --loom_combat_state.invulnerable_ticks;
        ++loom_combat_debug_epoch;
    }
    count = loom_actor_pool.count;
    if (count == 0u) {
        return LOOM_STATUS_OK;
    }
    /* Before the contact pass, so something a dart kills this tick cannot
     * also damage the player on its way out. These two passes need only the
     * pool, so a player who attacks without a Health component still lands
     * shots and a second player is still hurt; only the player's own
     * contact pass needs health to take. */
    if (loom_actor_pool.projectile_count != 0u) {
        loom_combat_projectiles(count);
    }
    loom_combat_second_player(count);
    /* Without a Health component the player cannot be hurt, but it can
     * still land on a stompable actor: the pass runs for the stomps alone. */
    hurtable = loom_generated_combat_enabled;
#if defined(LOOM_ACTOR_BODY_FAST)
    {
        /* body.asm finds the touching actor (reading the player's box
         * itself); C settles the touch, and reads the box only then. */
        loom_u16 start;
        loom_u16 found;

        start = 0u;
        for (;;) {
            found = loom_pvs_combat_touch(start, count,
                                          loom_combat_state.shot_type);
            if (found == 0xffffu) {
                break;
            }
            if (loom_movement_player_box(&player_left, &player_top,
                                         &player_right,
                                         &player_bottom) != LOOM_STATUS_OK) {
                return LOOM_STATUS_OK;
            }
            index = (loom_u8)found;
            if (loom_combat_touched(index, loom_actor_pool.type_ptr[index],
                                    shielded, hurtable,
                                    player_bottom) != LOOM_FALSE) {
                return LOOM_STATUS_OK;
            }
            start = (loom_u16)(found + 1u);
        }
    }
#else
    if (loom_movement_player_box(&player_left, &player_top, &player_right,
                                 &player_bottom) != LOOM_STATUS_OK) {
        return LOOM_STATUS_OK;
    }
    for (index = 0u; index < count; ++index) {
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
        if (type == loom_combat_state.shot_type) {
            continue;
        }
        if (loom_combat_touched(index, type, shielded, hurtable,
                                player_bottom) != LOOM_FALSE) {
            return LOOM_STATUS_OK;
        }
    }
#endif
    if (loom_combat_state.contact_index != LOOM_ACTOR_INVALID_INDEX) {
        loom_combat_state.contact_index = LOOM_ACTOR_INVALID_INDEX;
        ++loom_combat_debug_epoch;
    }
    return LOOM_STATUS_OK;
}

loom_u16 loom_combat_stomp_count(void)
{
    return loom_combat_state.stomp_count;
}

void loom_combat_debug_snapshot(LoomCombatDebugSnapshot *snapshot)
{
    snapshot->player_health = loom_combat_state.player_health;
    snapshot->damage_count = loom_combat_state.damage_count;
    snapshot->invulnerable_ticks = loom_combat_state.invulnerable_ticks;
    snapshot->game_over = loom_combat_state.game_over;
    snapshot->projectile_hits = loom_combat_state.projectile_hits;
    snapshot->stomp_count = loom_combat_state.stomp_count;
    snapshot->contact_index = loom_combat_state.contact_index;
    snapshot->reserved = 0u;
}
