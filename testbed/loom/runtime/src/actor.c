#include <loom/actor.h>
#include <loom/game.h>
#include <loom/movement.h>

LoomActorPool loom_actor_pool;

/* Set while an actor resolves its own move. Actors resolve against the
 * material grid, not against each other: without this an actor's own
 * solid box would block it, and pairwise actor tests would cost the
 * tick a quadratic sweep for no authored benefit. */
static loom_u8 loom_actor_resolving;

loom_u8 loom_actor_debug_epoch;

static const LoomActorType *loom_actor_type_at(loom_u8 index)
{
    return loom_actor_pool.type_ptr[index];
}

static LoomStatus loom_actor_validate_scene(const LoomActorScene *scene)
{
    loom_u8 index;

    if (scene->reserved != 0u || scene->instance_count > LOOM_ACTOR_CAPACITY ||
        (scene->instance_count != 0u &&
         scene->instances == (const LoomActorInstance *)0)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < scene->instance_count; ++index) {
        const LoomActorInstance *instance;
        const LoomActorType *type;

        instance = &scene->instances[index];
        if (instance->type_index >= loom_generated_actor_type_count ||
            instance->flags > LOOM_ACTOR_INSTANCE_FLAG_MAX ||
            instance->sprite_slot > LOOM_OAM_SLOT_MAX) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (instance->waypoint_count != 0u &&
            scene->waypoints == (const LoomActorWaypoint *)0) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        type = &loom_generated_actor_types[instance->type_index];
        if (type->body > LOOM_ACTOR_BODY_MAX ||
            type->collision > LOOM_ACTOR_COLLISION_MAX ||
            type->behavior > LOOM_ACTOR_BEHAVIOR_MAX) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (type->body == LOOM_ACTOR_BODY_PLATFORMER &&
            type->body_params >= loom_generated_actor_body_count) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (type->behavior == LOOM_ACTOR_BEHAVIOR_PATROL &&
            instance->waypoint_count < 2u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (type->behavior == LOOM_ACTOR_BEHAVIOR_CONTROLLER &&
            (type->body == LOOM_ACTOR_BODY_NONE || type->behavior_ticks == 0u ||
             type->behavior_ticks > LOOM_INPUT_PAD_CAPACITY)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (type->behavior == LOOM_ACTOR_BEHAVIOR_CHASE &&
            (type->behavior_sight == 0u ||
             type->behavior_lose < type->behavior_sight)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (type->contact_damage != 0u &&
            (type->hit_width == 0u || type->hit_height == 0u)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (type->body != LOOM_ACTOR_BODY_NONE && type->speed == 0u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    return LOOM_STATUS_OK;
}

/* Four directions clockwise from down, matching the animation slots. */
static void loom_actor_direction_vector(loom_u8 direction,
                                        loom_s8 *step_x,
                                        loom_s8 *step_y)
{
    switch (direction & 3u) {
    case 1u:
        *step_x = 1;
        *step_y = 0;
        break;
    case 2u:
        *step_x = 0;
        *step_y = -1;
        break;
    case 3u:
        *step_x = -1;
        *step_y = 0;
        break;
    default:
        *step_x = 0;
        *step_y = 1;
        break;
    }
}

/* What a controller actor reads this tick: the snapshot the move phase
 * handed over, which outlives the update. Null reads as no pad held. */
static const LoomInputSnapshot *loom_actor_input;
/* The first controller actor's slot, found once at activation, so the
 * camera's and the witness's question costs a compare, not a walk.
 * Exported for body.asm's loom_actor_controller_position. */
loom_u8 loom_actor_controller_slot = LOOM_ACTOR_INVALID_INDEX;
/* Set each tick by loom_actor_update while that slot's actor has not joined
 * and its pad is idle; body.asm's pass then skips the slot. */
loom_u8 loom_actor_controller_idle;
/* Where that actor last meant to go, so an attack while it stands still
 * launches the way it faces; zero until it has moved. */
static loom_s8 loom_actor_controller_facing_x = 0;
static loom_s8 loom_actor_controller_facing_y = 0;
/* The box a controller actor may not leave while a co-op camera frames two. */
static loom_u8 loom_actor_view_enabled = LOOM_FALSE;
static loom_s16 loom_actor_view_left;
static loom_s16 loom_actor_view_top;
static loom_s16 loom_actor_view_right;
static loom_s16 loom_actor_view_bottom;

/* What a room exit carries of the second player: whether it was joined,
 * and the health it had. */
static loom_u8 loom_actor_carry_noted = LOOM_FALSE;
static loom_u8 loom_actor_carry_health;

void loom_actor_note_second_player(void)
{
    loom_u8 slot;

    slot = loom_actor_controller_slot;
    loom_actor_carry_noted =
        (loom_u8)(slot != LOOM_ACTOR_INVALID_INDEX && slot < loom_actor_pool.count &&
                  loom_actor_pool.alive[slot] != LOOM_FALSE &&
                  loom_actor_pool.behavior_a[slot] != 0u);
    loom_actor_carry_health =
        loom_actor_carry_noted != LOOM_FALSE ? loom_actor_pool.health[slot] : 0u;
}

void loom_actor_restore_second_player(loom_s16 x, loom_s16 y)
{
    loom_u8 slot;

    slot = loom_actor_controller_slot;
    if (loom_actor_carry_noted == LOOM_FALSE || slot == LOOM_ACTOR_INVALID_INDEX ||
        slot >= loom_actor_pool.count || loom_actor_pool.alive[slot] == LOOM_FALSE) {
        loom_actor_carry_noted = LOOM_FALSE;
        return;
    }
    loom_actor_pool.x[slot] = x;
    loom_actor_pool.y[slot] = y;
    loom_actor_pool.sub_x[slot] = 0u;
    loom_actor_pool.sub_y[slot] = 0u;
    loom_actor_pool.behavior_a[slot] = 1u;
    if (loom_actor_pool.type_ptr[slot]->health != 0u && loom_actor_carry_health != 0u) {
        loom_actor_pool.health[slot] = loom_actor_carry_health;
    }
    (void)loom_mode1_set_sprite_position(loom_actor_pool.sprite_slot[slot], x, y);
    loom_actor_pool.sprite_sent_x[slot] = x;
    loom_actor_pool.sprite_sent_y[slot] = y;
    loom_actor_carry_noted = LOOM_FALSE;
    ++loom_actor_debug_epoch;
}

void loom_actor_set_view_box(loom_s16 left,
                             loom_s16 top,
                             loom_s16 right,
                             loom_s16 bottom,
                             loom_u8 enabled)
{
    loom_actor_view_enabled =
        (loom_u8)(enabled != LOOM_FALSE && left <= right && top <= bottom);
    loom_actor_view_left = left;
    loom_actor_view_top = top;
    loom_actor_view_right = right;
    loom_actor_view_bottom = bottom;
}
/* The packed list of solid actors the player's collision queries walk. */
static void loom_actor_solid_add(loom_u8 index);
static void loom_actor_solid_remove(loom_u8 index);

void loom_actor_set_input(const LoomInputSnapshot *input)
{
    loom_actor_input = input;
}

/* The pad drives the intent: the D-pad for a top-down body; for a platformer
 * body the run, and in intent_y a jump (-1 the tick B is pressed, 1 while it
 * is held, 0 once released so the rise is cut). */
static void loom_actor_behavior_controller(loom_u8 index,
                                           const LoomActorType *type)
{
    loom_u8 pad;
    loom_u16 held;
    loom_u16 pressed;

    pad = (loom_u8)(type->behavior_ticks != 0u ? type->behavior_ticks - 1u : 0u);
    if (pad >= LOOM_INPUT_PAD_CAPACITY ||
        loom_actor_input == (const LoomInputSnapshot *)0 ||
        pad >= loom_actor_input->pad_count) {
        loom_actor_pool.intent_x[index] = 0;
        loom_actor_pool.intent_y[index] = 0;
        return;
    }
    held = loom_actor_input->pads[pad].held;
    pressed = loom_actor_input->pads[pad].pressed;
    /* The first press joins this player: until then the camera frames
     * player one alone, so a placed second player costs solo play nothing. */
    if (held != 0u) {
        loom_actor_pool.behavior_a[index] = 1u;
    }
    loom_actor_pool.intent_x[index] =
        (loom_s8)(((held & LOOM_BUTTON_RIGHT) != 0u) -
                  ((held & LOOM_BUTTON_LEFT) != 0u));
    if (type->body == LOOM_ACTOR_BODY_PLATFORMER) {
        loom_actor_pool.intent_y[index] =
            (loom_s8)((pressed & LOOM_BUTTON_B) != 0u
                          ? -1
                          : ((held & LOOM_BUTTON_B) != 0u ? 1 : 0));
    } else {
        loom_actor_pool.intent_y[index] =
            (loom_s8)(((held & LOOM_BUTTON_DOWN) != 0u) -
                      ((held & LOOM_BUTTON_UP) != 0u));
    }
    /* A platformer faces along the ground; a top-down body faces the way
     * it walks, diagonals included, as player one does. */
    if (index == loom_actor_controller_slot) {
        if (type->body == LOOM_ACTOR_BODY_PLATFORMER) {
            if (loom_actor_pool.intent_x[index] != 0) {
                loom_actor_controller_facing_x = loom_actor_pool.intent_x[index];
                loom_actor_controller_facing_y = 0;
            }
        } else if (loom_actor_pool.intent_x[index] != 0 ||
                   loom_actor_pool.intent_y[index] != 0) {
            loom_actor_controller_facing_x = loom_actor_pool.intent_x[index];
            loom_actor_controller_facing_y = loom_actor_pool.intent_y[index];
        }
    }
}

#if !defined(LOOM_ACTOR_BODY_FAST)
/* body.asm answers this on the console: four calls a tick through 816-tcc
 * cost more than the question. */
loom_u8 loom_actor_controller_position(loom_s16 *x, loom_s16 *y)
{
    loom_u8 index;

    index = loom_actor_controller_slot;
    if (index == LOOM_ACTOR_INVALID_INDEX ||
        loom_actor_pool.initialized == LOOM_FALSE ||
        loom_actor_pool.alive[index] == LOOM_FALSE ||
        loom_actor_pool.behavior_a[index] == 0u) {
        return LOOM_FALSE;
    }
    *x = loom_actor_pool.x[index];
    *y = loom_actor_pool.y[index];
    return LOOM_TRUE;
}
#endif

loom_u8 loom_actor_controller_health(void)
{
    loom_u8 index;

    index = loom_actor_controller_slot;
    if (index == LOOM_ACTOR_INVALID_INDEX ||
        loom_actor_pool.initialized == LOOM_FALSE ||
        loom_actor_pool.alive[index] == LOOM_FALSE ||
        loom_actor_pool.behavior_a[index] == 0u) {
        return 0u;
    }
    return loom_actor_pool.health[index];
}

loom_u8 loom_actor_controller_aim(loom_s16 *x,
                                  loom_s16 *y,
                                  loom_s8 *facing_x,
                                  loom_s8 *facing_y,
                                  loom_u8 *pad)
{
    loom_u8 index;
    const LoomActorType *type;

    index = loom_actor_controller_slot;
    if (index == LOOM_ACTOR_INVALID_INDEX ||
        loom_actor_pool.initialized == LOOM_FALSE ||
        loom_actor_pool.alive[index] == LOOM_FALSE ||
        loom_actor_pool.behavior_a[index] == 0u) {
        return LOOM_FALSE;
    }
    type = loom_actor_pool.type_ptr[index];
    *x = loom_actor_pool.x[index];
    *y = loom_actor_pool.y[index];
    *pad = (loom_u8)(type->behavior_ticks != 0u ? type->behavior_ticks - 1u : 0u);
    if (loom_actor_controller_facing_x == 0 &&
        loom_actor_controller_facing_y == 0) {
        /* Not moved yet: a platformer looks right, a top-down body down,
         * where player one starts out looking too. */
        *facing_x = (loom_s8)(type->body == LOOM_ACTOR_BODY_PLATFORMER ? 1 : 0);
        *facing_y = (loom_s8)(type->body == LOOM_ACTOR_BODY_PLATFORMER ? 0 : 1);
    } else {
        *facing_x = loom_actor_controller_facing_x;
        *facing_y = loom_actor_controller_facing_y;
    }
    return LOOM_TRUE;
}

static void loom_actor_behavior_patrol(loom_u8 index,
                                       const LoomActorType *type)
{
    const LoomActorScene *scene;
    const LoomActorInstance *instance;
    const LoomActorWaypoint *target;
    loom_u8 step;
    loom_s16 delta_x;
    loom_s16 delta_y;

    scene = loom_actor_pool.scene;
    instance = &scene->instances[index];
    if (loom_actor_pool.behavior_b[index] != 0u) {
        --loom_actor_pool.behavior_b[index];
        loom_actor_pool.intent_x[index] = 0;
        loom_actor_pool.intent_y[index] = 0;
        return;
    }
    step = loom_actor_pool.behavior_a[index];
    if (step >= instance->waypoint_count) {
        step = 0u;
        loom_actor_pool.behavior_a[index] = 0u;
    }
    target = &scene->waypoints[instance->first_waypoint + step];
    delta_x = (loom_s16)(target->x - loom_actor_pool.x[index]);
    delta_y = (loom_s16)(target->y - loom_actor_pool.y[index]);
    /* A platformer walker patrols along x; gravity owns its y. */
    if (type->body == LOOM_ACTOR_BODY_PLATFORMER) {
        delta_y = 0;
    }
    if (delta_x == 0 && delta_y == 0) {
        /* Arrived: wait, then take the next leg. Ping-pong walks the route
         * back instead of jumping to its start. */
        loom_actor_pool.behavior_b[index] = type->behavior_ticks;
        loom_actor_pool.intent_x[index] = 0;
        loom_actor_pool.intent_y[index] = 0;
        if ((type->flags & LOOM_ACTOR_FLAG_PING_PONG) != 0u) {
            loom_u8 reverse;

            reverse = (loom_u8)(loom_actor_pool.blocked[index] & 0x80u);
            if (reverse != 0u) {
                if (step == 0u) {
                    loom_actor_pool.blocked[index] =
                        (loom_u8)(loom_actor_pool.blocked[index] & 0x7fu);
                    loom_actor_pool.behavior_a[index] = 1u;
                } else {
                    loom_actor_pool.behavior_a[index] = (loom_u8)(step - 1u);
                }
            } else if ((loom_u8)(step + 1u) >= instance->waypoint_count) {
                loom_actor_pool.blocked[index] =
                    (loom_u8)(loom_actor_pool.blocked[index] | 0x80u);
                loom_actor_pool.behavior_a[index] =
                    step == 0u ? 0u : (loom_u8)(step - 1u);
            } else {
                loom_actor_pool.behavior_a[index] = (loom_u8)(step + 1u);
            }
        } else {
            loom_actor_pool.behavior_a[index] =
                (loom_u8)(step + 1u) >= instance->waypoint_count
                    ? 0u
                    : (loom_u8)(step + 1u);
        }
        return;
    }
    loom_actor_pool.intent_x[index] =
        delta_x > 0 ? (loom_s8)1 : (delta_x < 0 ? (loom_s8)-1 : (loom_s8)0);
    loom_actor_pool.intent_y[index] =
        delta_y > 0 ? (loom_s8)1 : (delta_y < 0 ? (loom_s8)-1 : (loom_s8)0);
}

static void loom_actor_behavior_wander(loom_u8 index,
                                       const LoomActorType *type)
{
    loom_s8 step_x;
    loom_s8 step_y;

    if (loom_actor_pool.behavior_b[index] == 0u ||
        loom_actor_pool.blocked[index] != 0u) {
        /* Four directions on a plane; on a side view, left or right. */
        loom_actor_pool.behavior_a[index] =
            type->body == LOOM_ACTOR_BODY_PLATFORMER
                ? (loom_u8)(loom_rng_below(2u) * 2u + 1u)
                : (loom_u8)loom_rng_below(4u);
        loom_actor_pool.behavior_b[index] = type->behavior_ticks;
    } else {
        --loom_actor_pool.behavior_b[index];
    }
    loom_actor_direction_vector(loom_actor_pool.behavior_a[index], &step_x,
                                &step_y);
    loom_actor_pool.intent_x[index] = step_x;
    loom_actor_pool.intent_y[index] = step_y;
}

/* Walks toward the player while it is close enough to notice, and gives up
 * once the player is beyond the losing range. The ranges are compared on the
 * larger axis, which costs no multiply and reads as a square of attention. */
static void loom_actor_behavior_chase(loom_u8 index, const LoomActorType *type)
{
    loom_s16 delta_x;
    loom_s16 delta_y;
    loom_u16 reach_x;
    loom_u16 reach_y;
    loom_u16 reach;
    loom_u16 range;

    delta_x = (loom_s16)(loom_movement_player_x() - loom_actor_pool.x[index]);
    delta_y = (loom_s16)(loom_movement_player_y() - loom_actor_pool.y[index]);
    reach_x = (loom_u16)(delta_x < 0 ? -delta_x : delta_x);
    reach_y = (loom_u16)(delta_y < 0 ? -delta_y : delta_y);
    reach = reach_x > reach_y ? reach_x : reach_y;
    /* behavior_a remembers whether this actor is already interested, so the
     * two ranges give the chase hysteresis instead of a flickering edge. */
    range = loom_actor_pool.behavior_a[index] != 0u ? type->behavior_lose
                                                    : type->behavior_sight;
    if (reach > range) {
        loom_actor_pool.behavior_a[index] = 0u;
        loom_actor_pool.intent_x[index] = 0;
        loom_actor_pool.intent_y[index] = 0;
        return;
    }
    loom_actor_pool.behavior_a[index] = 1u;
    loom_actor_pool.intent_x[index] =
        delta_x > 0 ? (loom_s8)1 : (delta_x < 0 ? (loom_s8)-1 : (loom_s8)0);
    loom_actor_pool.intent_y[index] =
        delta_y > 0 ? (loom_s8)1 : (delta_y < 0 ? (loom_s8)-1 : (loom_s8)0);
}

static void loom_actor_behavior_bounce(loom_u8 index,
                                       const LoomActorType *type)
{
    if (loom_actor_pool.intent_x[index] == 0 &&
        loom_actor_pool.intent_y[index] == 0) {
        /* First tick: start along whichever axes the type bounces on. */
        loom_actor_pool.intent_x[index] =
            (type->flags & LOOM_ACTOR_FLAG_BOUNCE_X) != 0u ? (loom_s8)1
                                                           : (loom_s8)0;
        loom_actor_pool.intent_y[index] =
            (type->flags & LOOM_ACTOR_FLAG_BOUNCE_Y) != 0u ? (loom_s8)1
                                                           : (loom_s8)0;
        return;
    }
    if ((loom_actor_pool.blocked[index] & 0x01u) != 0u) {
        loom_actor_pool.intent_x[index] =
            (loom_s8)(0 - loom_actor_pool.intent_x[index]);
    }
    if ((loom_actor_pool.blocked[index] & 0x02u) != 0u) {
        loom_actor_pool.intent_y[index] =
            (loom_s8)(0 - loom_actor_pool.intent_y[index]);
    }
}

/* Moves one axis and reports whether the world stopped it. A type with no
 * box has nothing to resolve, so it walks through the grid untested. */
static loom_u8 loom_actor_move_axis(loom_u8 index,
                                    const LoomActorType *type,
                                    loom_u8 horizontal)
{
    loom_s16 next;
    loom_u8 next_sub;
    loom_s8 direction;
    loom_u8 boxed;

    direction = horizontal != LOOM_FALSE ? loom_actor_pool.intent_x[index]
                                         : loom_actor_pool.intent_y[index];
    if (direction == 0) {
        return LOOM_FALSE;
    }
    boxed = (loom_u8)(type->box_width != 0u && type->box_height != 0u);
    if (horizontal != LOOM_FALSE) {
        loom_movement_step(loom_actor_pool.x[index],
                           loom_actor_pool.sub_x[index], direction,
                           type->speed, &next, &next_sub);
        if (boxed != LOOM_FALSE &&
            loom_movement_box_blocked(next, next_sub, loom_actor_pool.y[index],
                                      loom_actor_pool.sub_y[index],
                                      type->box_x, type->box_y,
                                      type->box_width,
                                      type->box_height) !=
                LOOM_MOVEMENT_COLLISION_NONE) {
            return LOOM_TRUE;
        }
        loom_actor_pool.x[index] = next;
        loom_actor_pool.sub_x[index] = next_sub;
        return LOOM_FALSE;
    }
    loom_movement_step(loom_actor_pool.y[index], loom_actor_pool.sub_y[index],
                       direction, type->speed, &next, &next_sub);
    if (boxed != LOOM_FALSE &&
        loom_movement_box_blocked(loom_actor_pool.x[index],
                                  loom_actor_pool.sub_x[index], next, next_sub,
                                  type->box_x, type->box_y, type->box_width,
                                  type->box_height) !=
            LOOM_MOVEMENT_COLLISION_NONE) {
        return LOOM_TRUE;
    }
    loom_actor_pool.y[index] = next;
    loom_actor_pool.sub_y[index] = next_sub;
    return LOOM_FALSE;
}

/*
 * A platformer body in the pool (PHY-001): the player's body over the slot's
 * arrays, without a pad. The behavior's intent_x is the run; intent_y is
 * ignored, gravity owns y. A walker whose body turns at ledges treats the
 * edge ahead as a wall, which the bounce and wander behaviors already
 * answer by reversing. Returns the LOOM_ACTOR_BODY_FLAG_* state.
 */
#if defined(LOOM_ACTOR_BODY_FAST)
LOOM_STATIC_ASSERT(loom_actor_body_step_matches_body_asm,
                   sizeof(LoomActorBodyStep) == LOOM_ACTOR_BODY_STEP_BYTES);
LOOM_STATIC_ASSERT(loom_actor_type_matches_body_asm,
                   sizeof(LoomActorType) == LOOM_ACTOR_TYPE_BYTES);
LOOM_STATIC_ASSERT(loom_actor_instance_matches_body_asm,
                   sizeof(LoomActorInstance) == LOOM_ACTOR_INSTANCE_BYTES);
LOOM_STATIC_ASSERT(loom_actor_waypoint_matches_body_asm,
                   sizeof(LoomActorWaypoint) == LOOM_ACTOR_WAYPOINT_BYTES);
static LoomActorBodyStep loom_actor_body_step;
/* The actors the pass leaves to the C loop body this tick. */
static loom_u8 loom_actor_slow[LOOM_ACTOR_CAPACITY];

void loom_actor_drive_slot(loom_u16 index, loom_u16 key, loom_u16 air)
{
    (void)loom_generated_actor_drive_animation(
        loom_actor_pool.sprite_slot[index], (loom_u8)(key & 1u),
        loom_actor_pool.intent_x[index], loom_actor_pool.intent_y[index],
        (loom_u8)air);
    loom_actor_pool.animation_sent[index] = (loom_u8)key;
}

/* The console's step: body.asm does the ledge turn, the velocities, the X
 * move and the Y tile scan on the record; the floor merge, the slope under
 * the sensor and the snap are the same C as the portable rendition below. */
static loom_u8 loom_actor_platformer_step(loom_u8 index,
                                          const LoomActorType *type,
                                          const LoomPlatformerBody *body)
{
    LoomActorBodyStep *step;
    loom_u8 flags;
    loom_u8 grounded;
    loom_u8 cell;

    if (loom_actor_pool.solid_count == 0u) {
        /* Nothing to stand on but the tiles: the assembly finishes the
         * step itself, straight in the pool's arrays. */
        return loom_pvs_actor_step_bound((loom_u16)index, type, body);
    }
    step = &loom_actor_body_step;
    grounded = (loom_u8)(loom_actor_pool.body_flags[index] &
                         LOOM_ACTOR_BODY_FLAG_ON_GROUND);
    step->x = loom_actor_pool.x[index];
    step->y = loom_actor_pool.y[index];
    step->vx = loom_actor_pool.velocity_x[index];
    step->vy = loom_actor_pool.velocity_y[index];
    step->box_x = type->box_x;
    step->box_y = type->box_y;
    step->box_w = type->box_width;
    step->box_h = type->box_height;
    step->sub_x = loom_actor_pool.sub_x[index];
    step->sub_y = loom_actor_pool.sub_y[index];
    step->intent_x = loom_actor_pool.intent_x[index];
    step->intent_y = loom_actor_pool.intent_y[index];
    step->grounded = grounded != 0u ? LOOM_TRUE : LOOM_FALSE;
    step->turn_at_ledges =
        (loom_u8)((body->flags & LOOM_PLATFORMER_FLAG_TURN_AT_LEDGES) != 0u);
    loom_pvs_actor_body(step, body);
    loom_actor_pool.x[index] = step->x;
    loom_actor_pool.sub_x[index] = step->sub_x;
    loom_actor_pool.velocity_x[index] = step->vx;
    loom_actor_pool.velocity_y[index] = step->vy;
    if (step->jumped != LOOM_FALSE) {
        loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
        grounded = LOOM_FALSE;
    }
    flags = step->flags;
    if (step->phase == 1u) {
        loom_s16 landed;
        loom_s16 actor_floor;
        loom_u8 platform;

        landed = step->landed;
        platform = loom_actor_floor_below(step->left, step->right,
                                          (loom_s16)(step->bottom + 1),
                                          step->reach, index, &actor_floor);
        if (platform != LOOM_ACTOR_INVALID_INDEX &&
            (landed < 0 || actor_floor < landed)) {
            landed = actor_floor;
            loom_actor_pool.riding[index] = platform;
        } else {
            loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
        }
        LOOM_MOVEMENT_CELL_AT(cell, step->sensor_x, step->bottom);
        if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ||
            cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
            loom_s16 floor;

            floor = loom_movement_floor_in_cell(
                cell, step->sensor_x,
                (loom_s16)(step->bottom & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1)));
            if (landed < 0 || floor < landed) {
                landed = floor;
                loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
            }
        }
        if (landed >= 0 && landed <= step->reach) {
            loom_actor_pool.y[index] =
                (loom_s16)(landed - (loom_s16)type->box_height + 1 - type->box_y);
            loom_actor_pool.sub_y[index] = 0u;
            loom_actor_pool.velocity_y[index] = 0;
            flags |= LOOM_ACTOR_BODY_FLAG_ON_GROUND;
            if (grounded == LOOM_FALSE) {
                flags |= LOOM_ACTOR_BODY_FLAG_LANDED;
            }
        } else {
            loom_actor_pool.y[index] = step->next_y;
            loom_actor_pool.sub_y[index] = step->next_sub_y;
            loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
        }
    } else if (step->phase == 2u) {
        loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
        if (step->stopped >= 0) {
            loom_actor_pool.y[index] = (loom_s16)(step->stopped - type->box_y);
            loom_actor_pool.sub_y[index] = 0u;
            loom_actor_pool.velocity_y[index] = 0;
        } else {
            loom_actor_pool.y[index] = step->next_y;
            loom_actor_pool.sub_y[index] = step->next_sub_y;
        }
    } else {
        loom_actor_pool.y[index] = step->next_y;
        loom_actor_pool.sub_y[index] = step->next_sub_y;
    }
    loom_actor_pool.body_flags[index] = flags;
    return flags;
}
#else
static loom_u8 loom_actor_platformer_step(loom_u8 index,
                                          const LoomActorType *type,
                                          const LoomPlatformerBody *body)
{
    loom_s8 intent;
    loom_u8 grounded;
    loom_u8 flags;
    loom_s16 velocity;
    loom_s16 left;
    loom_s16 right;
    loom_s16 top;
    loom_s16 bottom;
    loom_s16 sensor_x;
    loom_s16 next;
    loom_u8 next_sub;
    loom_u8 cell;

    intent = loom_actor_pool.intent_x[index];
    grounded = (loom_u8)(loom_actor_pool.body_flags[index] &
                         LOOM_ACTOR_BODY_FLAG_ON_GROUND);
    flags = 0u;
    left = (loom_s16)(loom_actor_pool.x[index] + type->box_x);
    top = (loom_s16)(loom_actor_pool.y[index] + type->box_y);
    right = (loom_s16)(left + (loom_s16)type->box_width - 1);
    bottom = (loom_s16)(top + (loom_s16)type->box_height - 1);

    /* A ledge ahead is a wall to a walker that turns at them. */
    if (intent != 0 && grounded != LOOM_FALSE &&
        (body->flags & LOOM_PLATFORMER_FLAG_TURN_AT_LEDGES) != 0u) {
        loom_s16 ahead;

        ahead = (loom_s16)((intent > 0 ? right : left) + intent);
        LOOM_MOVEMENT_CELL_AT(cell, ahead, (loom_s16)(bottom + 1));
        if (cell == LOOM_MOVEMENT_COLLISION_NONE) {
            flags |= intent > 0 ? LOOM_ACTOR_BODY_FLAG_WALL_RIGHT
                                : LOOM_ACTOR_BODY_FLAG_WALL_LEFT;
            intent = 0;
            loom_actor_pool.velocity_x[index] = 0;
        }
    }

    /* Horizontal speed toward the intent, or friction on the ground. */
    velocity = loom_actor_pool.velocity_x[index];
    if (intent != 0) {
        loom_s16 gain;

        gain = (loom_s16)(grounded != LOOM_FALSE ? body->acceleration
                                                 : body->air_control);
        velocity = (loom_s16)(velocity + (intent > 0 ? gain : (loom_s16)-gain));
        if (velocity > (loom_s16)body->max_speed) {
            velocity = (loom_s16)body->max_speed;
        } else if (velocity < (loom_s16)-(loom_s16)body->max_speed) {
            velocity = (loom_s16)-(loom_s16)body->max_speed;
        }
    } else if (grounded != LOOM_FALSE) {
        if (velocity > 0) {
            velocity = (loom_s16)(velocity - (loom_s16)body->friction);
            if (velocity < 0) {
                velocity = 0;
            }
        } else if (velocity < 0) {
            velocity = (loom_s16)(velocity + (loom_s16)body->friction);
            if (velocity > 0) {
                velocity = 0;
            }
        }
    }
    loom_actor_pool.velocity_x[index] = velocity;
    velocity = loom_actor_pool.velocity_y[index];
    /* A controller's jump: leave the ground the tick B is pressed, and cut
     * the rise once B is released, so the height follows the hold. */
    if (loom_actor_pool.intent_y[index] < 0 && grounded != LOOM_FALSE) {
        velocity = (loom_s16)-(loom_s16)body->jump_speed;
        grounded = LOOM_FALSE;
        loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
    } else if (loom_actor_pool.intent_y[index] == 0 &&
               body->jump_cut < body->jump_speed &&
               velocity < (loom_s16)-(loom_s16)body->jump_cut) {
        velocity = (loom_s16)-(loom_s16)body->jump_cut;
    }
    if (grounded == LOOM_FALSE) {
        velocity = (loom_s16)(velocity + (loom_s16)body->gravity);
        if (velocity > (loom_s16)body->terminal_velocity) {
            velocity = (loom_s16)body->terminal_velocity;
        }
    }
    loom_actor_pool.velocity_y[index] = velocity;

    /* X against solid cells, stepping the feet's half tile while grounded. */
    next = loom_actor_pool.x[index];
    next_sub = loom_actor_pool.sub_x[index];
    LOOM_MOVEMENT_ADVANCE(next, next_sub, loom_actor_pool.velocity_x[index]);
    if (next != loom_actor_pool.x[index]) {
        loom_s16 delta;
        loom_s16 edge;
        loom_s16 probe;
        loom_s16 stop;
        loom_s16 wall_bottom;

        delta = (loom_s16)(next - loom_actor_pool.x[index]);
        edge = delta > 0 ? right : left;
        stop = next;
        wall_bottom = bottom;
        if (grounded != LOOM_FALSE) {
            wall_bottom = (loom_s16)(bottom - LOOM_MOVEMENT_TILE_PIXELS / 2);
            if (wall_bottom < top) {
                wall_bottom = top;
            }
        }
#if defined(LOOM_BODY_PROBES_FAST)
        probe = loom_pvs_body_probe_x(edge, delta, top, wall_bottom);
        if (probe != (loom_s16)0x7fff) {
            stop = (loom_s16)(loom_actor_pool.x[index] + (probe - edge) -
                              (delta > 0 ? 1 : -1));
            flags |= delta > 0 ? LOOM_ACTOR_BODY_FLAG_WALL_RIGHT
                               : LOOM_ACTOR_BODY_FLAG_WALL_LEFT;
            loom_actor_pool.velocity_x[index] = 0;
            next_sub = 0u;
        }
#else
        for (probe = (loom_s16)(edge + (delta > 0 ? 1 : -1));
             delta > 0 ? probe <= (loom_s16)(right + delta)
                       : probe >= (loom_s16)(left + delta);
             probe = (loom_s16)(probe + (delta > 0 ? 1 : -1))) {
            if (loom_movement_column_solid(probe, top, wall_bottom) != LOOM_FALSE) {
                stop = (loom_s16)(loom_actor_pool.x[index] + (probe - edge) -
                                  (delta > 0 ? 1 : -1));
                flags |= delta > 0 ? LOOM_ACTOR_BODY_FLAG_WALL_RIGHT
                                   : LOOM_ACTOR_BODY_FLAG_WALL_LEFT;
                loom_actor_pool.velocity_x[index] = 0;
                next_sub = 0u;
                break;
            }
            if (delta > 0 ? ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) !=
                             LOOM_MOVEMENT_TILE_PIXELS - 1)
                          : ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) != 0)) {
                probe = delta > 0
                            ? (loom_s16)(probe | (LOOM_MOVEMENT_TILE_PIXELS - 1))
                            : (loom_s16)(probe & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
            }
        }
#endif
        loom_actor_pool.x[index] = stop;
        left = (loom_s16)(stop + type->box_x);
        right = (loom_s16)(left + (loom_s16)type->box_width - 1);
    }
    loom_actor_pool.sub_x[index] = next_sub;
    sensor_x = (loom_s16)(left + (loom_s16)(type->box_width / 2u));

    /* Y: land on the first floor between the old feet and the new, or stop
     * under the first solid ceiling. */
    next = loom_actor_pool.y[index];
    next_sub = loom_actor_pool.sub_y[index];
    LOOM_MOVEMENT_ADVANCE(next, next_sub, loom_actor_pool.velocity_y[index]);
    if (loom_actor_pool.velocity_y[index] > 0 || grounded != LOOM_FALSE) {
        loom_s16 reach;
        loom_s16 row_top;
        loom_s16 landed;

        reach = (loom_s16)(next + type->box_y + (loom_s16)type->box_height - 1);
        if (grounded != LOOM_FALSE) {
            reach = (loom_s16)(reach + LOOM_MOVEMENT_TILE_PIXELS);
        }
#if defined(LOOM_BODY_PROBES_FAST)
        landed = loom_pvs_body_scan_floor(left, right, bottom, reach, sensor_x);
#else
        landed = -1;
        for (row_top = (loom_s16)((bottom + 1) & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
             row_top <= reach && landed < 0;
             row_top = (loom_s16)(row_top + LOOM_MOVEMENT_TILE_PIXELS)) {
            loom_s16 x;

            LOOM_MOVEMENT_CELL_AT(cell, sensor_x, row_top);
            if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ||
                cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
                loom_s16 floor;

                floor = loom_movement_floor_in_cell(cell, sensor_x, row_top);
                if (floor >= bottom - LOOM_MOVEMENT_TILE_PIXELS && floor <= reach) {
                    landed = floor;
                }
                continue;
            }
            for (x = left; x <= right;
                 x = (loom_s16)((x | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
                LOOM_MOVEMENT_CELL_AT(cell, x, row_top);
                if (cell == LOOM_MOVEMENT_COLLISION_SOLID ||
                    (cell == LOOM_MOVEMENT_COLLISION_ONE_WAY && bottom < row_top)) {
                    landed = (loom_s16)(row_top - 1);
                    break;
                }
            }
        }
#endif
        {
            loom_s16 actor_floor;
            loom_u8 platform;

            platform = loom_actor_floor_below(left, right, (loom_s16)(bottom + 1),
                                              reach, index, &actor_floor);
            if (platform != LOOM_ACTOR_INVALID_INDEX &&
                (landed < 0 || actor_floor < landed)) {
                landed = actor_floor;
                loom_actor_pool.riding[index] = platform;
            } else {
                loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
            }
        }
        LOOM_MOVEMENT_CELL_AT(cell, sensor_x, bottom);
        if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ||
            cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
            loom_s16 floor;

            floor = loom_movement_floor_in_cell(
                cell, sensor_x,
                (loom_s16)(bottom & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1)));
            if (landed < 0 || floor < landed) {
                landed = floor;
                loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
            }
        }
        if (landed >= 0 && landed <= reach) {
            loom_actor_pool.y[index] =
                (loom_s16)(landed - (loom_s16)type->box_height + 1 - type->box_y);
            loom_actor_pool.sub_y[index] = 0u;
            loom_actor_pool.velocity_y[index] = 0;
            flags |= LOOM_ACTOR_BODY_FLAG_ON_GROUND;
            if (grounded == LOOM_FALSE) {
                flags |= LOOM_ACTOR_BODY_FLAG_LANDED;
            }
        } else {
            loom_actor_pool.y[index] = next;
            loom_actor_pool.sub_y[index] = next_sub;
            loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
        }
    } else if (loom_actor_pool.velocity_y[index] < 0) {
        loom_s16 reach;
        loom_s16 row_top;
        loom_s16 stopped;

        loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
        reach = (loom_s16)(next + type->box_y);
#if defined(LOOM_BODY_PROBES_FAST)
        stopped = loom_pvs_body_scan_ceiling(left, right, top, reach);
#else
        stopped = -1;
        for (row_top = (loom_s16)((top - 1) & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
             row_top + (LOOM_MOVEMENT_TILE_PIXELS - 1) >= reach && stopped < 0;
             row_top = (loom_s16)(row_top - LOOM_MOVEMENT_TILE_PIXELS)) {
            loom_s16 x;

            for (x = left; x <= right;
                 x = (loom_s16)((x | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
                LOOM_MOVEMENT_CELL_AT(cell, x, row_top);
                if (cell == LOOM_MOVEMENT_COLLISION_SOLID) {
                    stopped = (loom_s16)(row_top + LOOM_MOVEMENT_TILE_PIXELS);
                    break;
                }
            }
        }
#endif
        if (stopped >= 0) {
            loom_actor_pool.y[index] = (loom_s16)(stopped - type->box_y);
            loom_actor_pool.sub_y[index] = 0u;
            loom_actor_pool.velocity_y[index] = 0;
        } else {
            loom_actor_pool.y[index] = next;
            loom_actor_pool.sub_y[index] = next_sub;
        }
    } else {
        loom_actor_pool.y[index] = next;
        loom_actor_pool.sub_y[index] = next_sub;
    }
    loom_actor_pool.body_flags[index] = flags;
    return flags;
}
#endif

static void loom_actor_reset_slot(loom_u8 index)
{
    loom_actor_pool.alive[index] = LOOM_FALSE;
    loom_actor_pool.type_index[index] = 0u;
    loom_actor_pool.sprite_slot[index] = 0u;
    loom_actor_pool.sub_x[index] = 0u;
    loom_actor_pool.sub_y[index] = 0u;
    loom_actor_pool.behavior_a[index] = 0u;
    loom_actor_pool.behavior_b[index] = 0u;
    loom_actor_pool.blocked[index] = 0u;
    loom_actor_pool.intent_x[index] = 0;
    loom_actor_pool.intent_y[index] = 0;
    loom_actor_pool.x[index] = 0;
    loom_actor_pool.y[index] = 0;
    loom_actor_pool.inert[index] = LOOM_FALSE;
    loom_actor_pool.health[index] = 0u;
    loom_actor_pool.velocity_x[index] = 0;
    loom_actor_pool.velocity_y[index] = 0;
    loom_actor_pool.body_flags[index] = 0u;
    loom_actor_pool.body_ptr[index] = (const LoomPlatformerBody *)0;
    loom_actor_pool.riding[index] = LOOM_ACTOR_INVALID_INDEX;
    loom_actor_pool.sprite_sent_x[index] = (loom_s16)0x7fff;
    loom_actor_pool.sprite_sent_y[index] = (loom_s16)0x7fff;
    loom_actor_pool.animation_sent[index] = 0xffu;
    loom_actor_pool.awake[index] = LOOM_FALSE;
}

LoomStatus loom_actor_initialize(void)
{
    loom_u8 index;

    for (index = 0u; index < LOOM_ACTOR_CAPACITY; ++index) {
        loom_actor_reset_slot(index);
    }
    loom_actor_pool.count = 0u;
    loom_actor_pool.projectile_count = 0u;
    loom_actor_pool.solid_count = 0u;
    loom_movement_solid_actors = 0u;
    loom_actor_pool.scene = (const LoomActorScene *)0;
    loom_actor_pool.initialized = LOOM_TRUE;
    loom_actor_resolving = LOOM_FALSE;
    ++loom_actor_debug_epoch;
    if (loom_generated_actors_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    return loom_actor_activate_scene(&loom_generated_actor_initial_scene);
}

LoomStatus loom_actor_activate_scene(const LoomActorScene *scene)
{
    loom_u8 index;
    loom_u8 stale;

    if (loom_actor_pool.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    /* Only slots that held or will hold an actor need clearing: the rest
     * were cleared at boot and nothing has touched them since. A scene
     * change is the busiest tick a room has, and resetting all 32 slots
     * there once cost it a frame. */
    stale = loom_actor_pool.count;
    if (scene != (const LoomActorScene *)0 && scene->instance_count > stale) {
        stale = scene->instance_count;
    }
    if (stale > LOOM_ACTOR_CAPACITY) {
        stale = LOOM_ACTOR_CAPACITY;
    }
    for (index = 0u; index < stale; ++index) {
        loom_actor_reset_slot(index);
    }
    loom_actor_pool.count = 0u;
    loom_actor_pool.projectile_count = 0u;
    loom_actor_controller_slot = LOOM_ACTOR_INVALID_INDEX;
    loom_actor_controller_facing_x = 0;
    loom_actor_controller_facing_y = 0;
    loom_actor_view_enabled = LOOM_FALSE;
    loom_actor_pool.solid_count = 0u;
    loom_movement_solid_actors = 0u;
    loom_actor_pool.scene = scene;
    ++loom_actor_debug_epoch;
    if (scene == (const LoomActorScene *)0) {
        return LOOM_STATUS_OK;
    }
    {
        LoomStatus status;

        status = loom_actor_validate_scene(scene);
        if (status != LOOM_STATUS_OK) {
            loom_actor_pool.scene = (const LoomActorScene *)0;
            return status;
        }
    }
#if defined(LOOM_ACTOR_BODY_FAST)
    /* The pool's arrays never move; the scene's tables change here. */
    loom_pvs_actor_bind(loom_actor_pool.x, loom_actor_pool.y, loom_actor_pool.sub_x,
                        loom_actor_pool.sub_y, loom_actor_pool.velocity_x,
                        loom_actor_pool.velocity_y, loom_actor_pool.intent_x,
                        loom_actor_pool.intent_y, loom_actor_pool.body_flags,
                        loom_actor_pool.riding);
    loom_pvs_actor_bind_pass(loom_actor_pool.alive, loom_actor_pool.inert,
                             loom_actor_pool.awake, loom_actor_pool.type_ptr,
                             loom_actor_pool.body_ptr, loom_actor_pool.behavior_a,
                             loom_actor_pool.behavior_b, loom_actor_pool.blocked,
                             loom_actor_pool.sprite_index,
                             loom_actor_pool.sprite_sent_x,
                             loom_actor_pool.sprite_sent_y,
                             loom_actor_pool.animation_sent, scene->instances,
                             scene->waypoints);
    loom_pvs_solid_bind(loom_actor_pool.solid_slots,
                        loom_mode1_visibility_table());
#endif
    for (index = 0u; index < scene->instance_count; ++index) {
        const LoomActorInstance *instance;
        const LoomActorType *type;

        instance = &scene->instances[index];
        type = &loom_generated_actor_types[instance->type_index];
        /* A pooled slot exists so `loom_actor_spawn` has somewhere to put a
         * projectile. It starts dead, hidden, and out of the solid count. */
        loom_actor_pool.alive[index] =
            (loom_u8)((instance->flags & LOOM_ACTOR_INSTANCE_FLAG_POOLED) == 0u);
        loom_actor_pool.type_index[index] = instance->type_index;
        loom_actor_pool.type_ptr[index] = type;
        loom_actor_pool.body_ptr[index] =
            type->body_params != LOOM_ACTOR_INVALID_INDEX
                ? &loom_generated_actor_bodies[type->body_params]
                : (const LoomPlatformerBody *)0;
        loom_actor_pool.sprite_slot[index] = instance->sprite_slot;
        {
            loom_s16 sprite;

            sprite = loom_mode1_slot_sprite_index(instance->sprite_slot);
            loom_actor_pool.sprite_index[index] =
                sprite < 0 ? 0xffu : (loom_u8)sprite;
        }
        loom_actor_pool.x[index] = instance->spawn_x;
        loom_actor_pool.y[index] = instance->spawn_y;
        /* The generated sprite table already draws this one where it stands
         * and nothing will move it, so the tick can pass it by. */
        loom_actor_pool.health[index] = type->health;
        loom_actor_pool.inert[index] =
            (loom_u8)(type->body == LOOM_ACTOR_BODY_NONE &&
                      type->behavior == LOOM_ACTOR_BEHAVIOR_STATIC &&
                      type->health == 0u);
        if (loom_actor_pool.alive[index] == LOOM_FALSE) {
            loom_actor_pool.inert[index] = LOOM_FALSE;
            (void)loom_mode1_set_sprite_visible(instance->sprite_slot,
                                                LOOM_FALSE);
            continue;
        }
        /* Placed awake: the first update sleeps whoever is off the screen. */
        loom_actor_pool.awake[index] = LOOM_TRUE;
        if (type->behavior == LOOM_ACTOR_BEHAVIOR_PROJECTILE) {
            ++loom_actor_pool.projectile_count;
        }
        if (type->collision == LOOM_ACTOR_COLLISION_SOLID) {
            loom_actor_solid_add(index);
        }
        if (type->behavior == LOOM_ACTOR_BEHAVIOR_CONTROLLER &&
            loom_actor_controller_slot == LOOM_ACTOR_INVALID_INDEX) {
            loom_actor_controller_slot = index;
        }
    }
    loom_actor_pool.count = scene->instance_count;
    loom_actor_pool.spawn_failures = 0u;
    return LOOM_STATUS_OK;
}

loom_u8 loom_actor_despawn(loom_u8 index)
{
    if (index >= loom_actor_pool.count ||
        loom_actor_pool.alive[index] == LOOM_FALSE) {
        return LOOM_FALSE;
    }
    loom_actor_pool.alive[index] = LOOM_FALSE;
    loom_actor_pool.intent_x[index] = 0;
    loom_actor_pool.intent_y[index] = 0;
    if (loom_actor_type_at(index)->behavior == LOOM_ACTOR_BEHAVIOR_PROJECTILE &&
        loom_actor_pool.projectile_count != 0u) {
        --loom_actor_pool.projectile_count;
    }
    if (loom_actor_type_at(index)->collision == LOOM_ACTOR_COLLISION_SOLID) {
        loom_actor_solid_remove(index);
    }
    (void)loom_mode1_set_sprite_visible(loom_actor_pool.sprite_slot[index],
                                        LOOM_FALSE);
    ++loom_actor_debug_epoch;
    return LOOM_TRUE;
}

loom_u8 loom_actor_spawn(loom_u8 type_index,
                         loom_s16 x,
                         loom_s16 y,
                         loom_s8 direction_x,
                         loom_s8 direction_y)
{
    loom_u8 index;

    if (loom_actor_pool.initialized == LOOM_FALSE ||
        type_index >= loom_generated_actor_type_count) {
        return LOOM_ACTOR_INVALID_INDEX;
    }
    /* Nothing is allocated during play: a spawn takes a slot the scene
     * already reserved for this type, or it fails and says so. */
    for (index = 0u; index < loom_actor_pool.count; ++index) {
        const LoomActorType *type;

        if (loom_actor_pool.alive[index] != LOOM_FALSE ||
            loom_actor_pool.type_index[index] != type_index) {
            continue;
        }
        type = &loom_generated_actor_types[type_index];
        loom_actor_pool.alive[index] = LOOM_TRUE;
        loom_actor_pool.x[index] = x;
        loom_actor_pool.y[index] = y;
        loom_actor_pool.sub_x[index] = 0u;
        loom_actor_pool.sub_y[index] = 0u;
        loom_actor_pool.intent_x[index] = direction_x;
        loom_actor_pool.intent_y[index] = direction_y;
        loom_actor_pool.behavior_a[index] = 0u;
        loom_actor_pool.behavior_b[index] = type->behavior_ticks;
        loom_actor_pool.blocked[index] = 0u;
        loom_actor_pool.health[index] = type->health;
        loom_actor_pool.inert[index] = LOOM_FALSE;
        loom_actor_pool.awake[index] = LOOM_TRUE;
        if (type->behavior == LOOM_ACTOR_BEHAVIOR_PROJECTILE) {
            ++loom_actor_pool.projectile_count;
        }
        if (type->collision == LOOM_ACTOR_COLLISION_SOLID) {
            loom_actor_solid_add(index);
        }
        /* Placed, and remembered where, so the first update after this
         * makes no call for an actor that has not moved; no helper call
         * here, since activation is the busiest tick a room has. */
        (void)loom_mode1_set_sprite_position(loom_actor_pool.sprite_slot[index],
                                             x, y);
        loom_actor_pool.sprite_sent_x[index] = x;
        loom_actor_pool.sprite_sent_y[index] = y;
        (void)loom_mode1_set_sprite_visible(loom_actor_pool.sprite_slot[index],
                                            LOOM_TRUE);
        ++loom_actor_debug_epoch;
        return index;
    }
    if (loom_actor_pool.spawn_failures != 0xffffu) {
        ++loom_actor_pool.spawn_failures;
    }
    ++loom_actor_debug_epoch;
    return LOOM_ACTOR_INVALID_INDEX;
}

loom_u16 loom_actor_spawn_failures(void)
{
    return loom_actor_pool.spawn_failures;
}

/* The spawn window and the epoch flag of the update in progress: the loop
 * body reads them as statics so the console's pass can hand it single slots
 * without stacking arguments. */
static loom_s16 loom_actor_window_left;
static loom_s16 loom_actor_window_top;
static loom_s16 loom_actor_window_right;
static loom_s16 loom_actor_window_bottom;
static loom_u8 loom_actor_stepped;

/* The spawn window: the view, widened so an actor is already moving by the
 * time the camera can see it. A streamed room is many screens across, so
 * without this every placed actor would cost the tick whether or not it
 * could be seen. A sleeping actor keeps its state and resumes where it
 * left off. */
static void loom_actor_window(void)
{
    loom_actor_window_left =
        (loom_s16)(loom_mode1_camera_x() - LOOM_ACTOR_SPAWN_WINDOW_MARGIN);
    loom_actor_window_top =
        (loom_s16)(loom_mode1_camera_y() - LOOM_ACTOR_SPAWN_WINDOW_MARGIN);
    loom_actor_window_right = (loom_s16)(loom_actor_window_left + (loom_s16)LOOM_MODE1_VIEW_WIDTH +
                              (loom_s16)(2 * LOOM_ACTOR_SPAWN_WINDOW_MARGIN));
    loom_actor_window_bottom = (loom_s16)(loom_actor_window_top + (loom_s16)LOOM_MODE1_VIEW_HEIGHT +
                               (loom_s16)(2 * LOOM_ACTOR_SPAWN_WINDOW_MARGIN));
}

/* One actor's tick: sleep or wake against the window, the behavior, the
 * body step, the carry of riders, the sprite and the animation key. */
void loom_actor_carry_riders(loom_u16 index, loom_s16 delta_x,
                             loom_s16 delta_y)
{
    loom_u8 rider;

    for (rider = 0u; rider < loom_actor_pool.count; ++rider) {
        if (loom_actor_pool.riding[rider] == index &&
            loom_actor_pool.alive[rider] != LOOM_FALSE) {
            loom_actor_pool.x[rider] =
                (loom_s16)(loom_actor_pool.x[rider] + delta_x);
            loom_actor_pool.y[rider] =
                (loom_s16)(loom_actor_pool.y[rider] + delta_y);
        }
    }
    if (loom_movement_riding() == index) {
        loom_movement_carry(delta_x, delta_y);
    }
}

static LoomStatus loom_actor_update_slot(loom_u8 index)
{
    const LoomActorType *type;
    LoomStatus status;
    loom_u8 blocked;
    loom_s16 before_x;
    loom_s16 before_y;

    if (loom_actor_pool.alive[index] == LOOM_FALSE ||
        loom_actor_pool.inert[index] != LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    type = loom_actor_type_at(index);
    before_x = loom_actor_pool.x[index];
    before_y = loom_actor_pool.y[index];
    /* A second player nobody has picked up is scenery: it keeps its
     * authored frame where it was placed and costs the tick nothing
     * until its pad is first pressed, which the behaviour notices from
     * the pad itself before any of its own work. */
    if (type->behavior == LOOM_ACTOR_BEHAVIOR_CONTROLLER &&
        loom_actor_pool.behavior_a[index] == 0u) {
        loom_u8 pad;

        pad = (loom_u8)(type->behavior_ticks != 0u ? type->behavior_ticks - 1u : 0u);
        if (loom_actor_input == (const LoomInputSnapshot *)0 ||
            pad >= LOOM_INPUT_PAD_CAPACITY || pad >= loom_actor_input->pad_count ||
            loom_actor_input->pads[pad].held == 0u) {
            return LOOM_STATUS_OK;
        }
    }
    if (loom_actor_pool.x[index] < loom_actor_window_left ||
        loom_actor_pool.x[index] > loom_actor_window_right ||
        loom_actor_pool.y[index] < loom_actor_window_top ||
        loom_actor_pool.y[index] > loom_actor_window_bottom) {
        /* A walker outside the window waits for the camera. A projectile
         * has left play, and sleeping one would hold its slot forever. */
        if (type->behavior == LOOM_ACTOR_BEHAVIOR_PROJECTILE) {
            (void)loom_actor_despawn(index);
            loom_actor_stepped = LOOM_TRUE;
        } else {
            ++loom_actor_pool.sleeping_count;
            if (loom_actor_pool.awake[index] != LOOM_FALSE) {
                /* Falling asleep: the clip stops where it is, so the
                 * animation pass costs nothing for it until it wakes. */
                loom_actor_pool.awake[index] = LOOM_FALSE;
                (void)loom_generated_actor_set_animation_playing(
                    loom_actor_pool.sprite_slot[index], LOOM_FALSE);
            }
        }
        return LOOM_STATUS_OK;
    }
    if (loom_actor_pool.awake[index] == LOOM_FALSE) {
        loom_actor_pool.awake[index] = LOOM_TRUE;
        (void)loom_generated_actor_set_animation_playing(
            loom_actor_pool.sprite_slot[index], LOOM_TRUE);
    }
    switch (type->behavior) {
    case LOOM_ACTOR_BEHAVIOR_PATROL:
        loom_actor_behavior_patrol(index, type);
        break;
    case LOOM_ACTOR_BEHAVIOR_WANDER:
        loom_actor_behavior_wander(index, type);
        break;
    case LOOM_ACTOR_BEHAVIOR_BOUNCE:
        loom_actor_behavior_bounce(index, type);
        break;
    case LOOM_ACTOR_BEHAVIOR_CHASE:
        loom_actor_behavior_chase(index, type);
        break;
    case LOOM_ACTOR_BEHAVIOR_CONTROLLER:
        loom_actor_behavior_controller(index, type);
        break;
    case LOOM_ACTOR_BEHAVIOR_PROJECTILE:
        /* It keeps the direction it was spawned with; all it decides is
         * whether it has any life left. */
        if (loom_actor_pool.behavior_b[index] == 0u) {
            (void)loom_actor_despawn(index);
            loom_actor_stepped = LOOM_TRUE;
            return LOOM_STATUS_OK;
        }
        --loom_actor_pool.behavior_b[index];
        break;
    default:
        loom_actor_pool.intent_x[index] = 0;
        loom_actor_pool.intent_y[index] = 0;
        break;
    }
    blocked = 0u;
    if (loom_actor_pool.intent_x[index] == 0 &&
        loom_actor_pool.intent_y[index] == 0 &&
        loom_actor_pool.velocity_x[index] == 0 &&
        loom_actor_pool.velocity_y[index] == 0 &&
        loom_actor_pool.riding[index] == LOOM_ACTOR_INVALID_INDEX &&
        (type->body != LOOM_ACTOR_BODY_PLATFORMER ||
         (loom_actor_pool.body_flags[index] & LOOM_ACTOR_BODY_FLAG_ON_GROUND) != 0u)) {
        /* An actor standing still on solid ground costs the tick no
         * body step: a ferry waiting at its dock, a bat with nothing in
         * sight, a placed second player nobody has picked up. */
        loom_actor_pool.body_flags[index] =
            (loom_u8)(loom_actor_pool.body_flags[index] & (loom_u8)~LOOM_ACTOR_BODY_FLAG_LANDED);
    } else if (type->body == LOOM_ACTOR_BODY_PLATFORMER &&
        loom_actor_pool.body_ptr[index] != (const LoomPlatformerBody *)0) {
        loom_u8 flags;

        flags = loom_actor_platformer_step(index, type,
                                           loom_actor_pool.body_ptr[index]);
        if ((flags & (LOOM_ACTOR_BODY_FLAG_WALL_LEFT |
                      LOOM_ACTOR_BODY_FLAG_WALL_RIGHT)) != 0u) {
            blocked |= 0x01u;
        }
    } else if (type->body != LOOM_ACTOR_BODY_NONE) {
        loom_actor_resolving = LOOM_TRUE;
        if (loom_actor_move_axis(index, type, LOOM_TRUE) != LOOM_FALSE) {
            blocked |= 0x01u;
        }
        if (loom_actor_move_axis(index, type, LOOM_FALSE) != LOOM_FALSE) {
            blocked |= 0x02u;
        }
        loom_actor_resolving = LOOM_FALSE;
    }
    /* A solid actor that moved carries whatever stands on it: riders in
     * the pool, and the player, before their own steps. */
    if (type->collision == LOOM_ACTOR_COLLISION_SOLID &&
        (loom_actor_pool.x[index] != before_x ||
         loom_actor_pool.y[index] != before_y)) {
        loom_actor_carry_riders((loom_u16)index,
                                (loom_s16)(loom_actor_pool.x[index] - before_x),
                                (loom_s16)(loom_actor_pool.y[index] - before_y));
    }
    /* Patrol keeps its direction flag in the high bit. */
    loom_actor_pool.blocked[index] =
        (loom_u8)((loom_actor_pool.blocked[index] & 0x80u) | blocked);
    if (type->behavior == LOOM_ACTOR_BEHAVIOR_PROJECTILE && blocked != 0u) {
        /* Something solid stopped it, so it is spent. */
        (void)loom_actor_despawn(index);
        loom_actor_stepped = LOOM_TRUE;
        return LOOM_STATUS_OK;
    }
    if (loom_actor_view_enabled != LOOM_FALSE &&
        type->behavior == LOOM_ACTOR_BEHAVIOR_CONTROLLER) {
        /* A second player stays on the co-op camera's screen. */
        if (loom_actor_pool.x[index] < loom_actor_view_left) {
            loom_actor_pool.x[index] = loom_actor_view_left;
            loom_actor_pool.sub_x[index] = 0u;
        } else if (loom_actor_pool.x[index] > loom_actor_view_right) {
            loom_actor_pool.x[index] = loom_actor_view_right;
            loom_actor_pool.sub_x[index] = 0u;
        }
        if (loom_actor_pool.y[index] < loom_actor_view_top) {
            loom_actor_pool.y[index] = loom_actor_view_top;
            loom_actor_pool.sub_y[index] = 0u;
        } else if (loom_actor_pool.y[index] > loom_actor_view_bottom) {
            loom_actor_pool.y[index] = loom_actor_view_bottom;
            loom_actor_pool.sub_y[index] = 0u;
        }
    }
    if (loom_actor_pool.x[index] != loom_actor_pool.sprite_sent_x[index] ||
        loom_actor_pool.y[index] != loom_actor_pool.sprite_sent_y[index]) {
        status = loom_mode1_set_sprite_position(
            loom_actor_pool.sprite_slot[index], loom_actor_pool.x[index],
            loom_actor_pool.y[index]);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        loom_actor_pool.sprite_sent_x[index] = loom_actor_pool.x[index];
        loom_actor_pool.sprite_sent_y[index] = loom_actor_pool.y[index];
    }
    {
        loom_u8 air;
        loom_u8 key;

        air = LOOM_BODY_AIR_GROUND;
        if (type->body == LOOM_ACTOR_BODY_PLATFORMER) {
            loom_u8 flags;

            flags = loom_actor_pool.body_flags[index];
            if ((flags & LOOM_ACTOR_BODY_FLAG_LANDED) != 0u) {
                air = LOOM_BODY_AIR_LANDED;
            } else if ((flags & LOOM_ACTOR_BODY_FLAG_ON_GROUND) == 0u) {
                air = loom_actor_pool.velocity_y[index] < 0
                          ? LOOM_BODY_AIR_RISING
                          : LOOM_BODY_AIR_FALLING;
            }
        }
        /* Driven only when what it would say has changed; the
         * animation module returns a finished landing clip to the
         * ground state on its own. */
        key = (loom_u8)((loom_actor_pool.intent_x[index] != 0 ||
                                 loom_actor_pool.intent_y[index] != 0
                             ? 1u
                             : 0u) |
                        ((loom_u8)(loom_actor_pool.intent_x[index] + 1) << 1) |
                        ((loom_u8)(loom_actor_pool.intent_y[index] + 1) << 3) |
                        (loom_u8)(air << 5));
        if (key != loom_actor_pool.animation_sent[index]) {
            status = loom_generated_actor_drive_animation(
                loom_actor_pool.sprite_slot[index], (loom_u8)(key & 1u),
                loom_actor_pool.intent_x[index],
                loom_actor_pool.intent_y[index], air);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            loom_actor_pool.animation_sent[index] = key;
        }
    }
    loom_actor_stepped = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

LoomStatus loom_actor_update(void)
{
    loom_u8 index;

    if (loom_actor_pool.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_actor_pool.scene == (const LoomActorScene *)0) {
        return LOOM_STATUS_OK;
    }
    loom_actor_pool.sleeping_count = 0u;
    loom_actor_stepped = LOOM_FALSE;
#if defined(LOOM_ACTOR_BODY_FAST)
    {
        /* The pass takes the common actors (its window comes from the
         * camera through the binding, solid actors' decks from the pool's
         * floor query); the slow list is the rest, solid actors included,
         * since the C loop body carries their riders. */
        loom_u8 out[4];
        loom_u8 position;
        loom_u8 slot;

        /* A second player nobody has picked up does nothing until its pad is
         * first pressed (loom_actor_update_slot's early return); decided
         * here once, the pass skips its slot rather than sending it to the
         * C loop body every tick. */
        loom_actor_controller_idle = LOOM_FALSE;
        slot = loom_actor_controller_slot;
        if (slot != LOOM_ACTOR_INVALID_INDEX &&
            loom_actor_pool.behavior_a[slot] == 0u) {
            const LoomActorType *type;
            loom_u8 pad;

            type = loom_actor_pool.type_ptr[slot];
            pad = (loom_u8)(type->behavior_ticks != 0u ? type->behavior_ticks - 1u : 0u);
            if (loom_actor_input == (const LoomInputSnapshot *)0 ||
                pad >= LOOM_INPUT_PAD_CAPACITY || pad >= loom_actor_input->pad_count ||
                loom_actor_input->pads[pad].held == 0u) {
                loom_actor_controller_idle = LOOM_TRUE;
            }
        }
        loom_pvs_actor_pass(loom_actor_pool.count, loom_actor_slow, out);
        if (out[0] != 0u) {
            loom_actor_window();
        }
        for (position = 0u; position < out[0]; ++position) {
            LoomStatus status;

            status = loom_actor_update_slot(loom_actor_slow[position]);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
        loom_actor_pool.sleeping_count =
            (loom_u8)(loom_actor_pool.sleeping_count + out[1]);
        if (out[2] != 0u) {
            loom_actor_stepped = LOOM_TRUE;
        }
        (void)index;
    }
#else
    {
        loom_actor_window();
        for (index = 0u; index < loom_actor_pool.count; ++index) {
            LoomStatus status;

            status = loom_actor_update_slot(index);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    }
#endif
    /* A scene whose every actor is inert never restates the witness. */
    if (loom_actor_stepped != LOOM_FALSE) {
        ++loom_actor_debug_epoch;
    }
    return LOOM_STATUS_OK;
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

static void loom_actor_solid_add(loom_u8 index)
{
    if (loom_actor_pool.solid_count < LOOM_ACTOR_CAPACITY) {
        loom_actor_pool.solid_slots[loom_actor_pool.solid_count] = index;
        ++loom_actor_pool.solid_count;
        loom_movement_solid_actors = loom_actor_pool.solid_count;
    }
}

static void loom_actor_solid_remove(loom_u8 index)
{
    loom_u8 position;

    for (position = 0u; position < loom_actor_pool.solid_count; ++position) {
        if (loom_actor_pool.solid_slots[position] == index) {
            --loom_actor_pool.solid_count;
            loom_movement_solid_actors = loom_actor_pool.solid_count;
            loom_actor_pool.solid_slots[position] =
                loom_actor_pool.solid_slots[loom_actor_pool.solid_count];
            return;
        }
    }
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

loom_u8 loom_actor_sleeping_count(void)
{
    return loom_actor_pool.sleeping_count;
}

loom_u8 loom_actor_count(void)
{
    return loom_actor_pool.count;
}

loom_s16 loom_actor_x(loom_u8 index)
{
    return index < loom_actor_pool.count ? loom_actor_pool.x[index] : 0;
}

loom_s16 loom_actor_y(loom_u8 index)
{
    return index < loom_actor_pool.count ? loom_actor_pool.y[index] : 0;
}

loom_u8 loom_actor_alive(loom_u8 index)
{
    return index < loom_actor_pool.count ? loom_actor_pool.alive[index]
                                         : LOOM_FALSE;
}

const LoomActorType *loom_actor_type(loom_u8 index)
{
    if (index >= loom_actor_pool.count ||
        loom_actor_pool.alive[index] == LOOM_FALSE) {
        return (const LoomActorType *)0;
    }
    return loom_actor_type_at(index);
}

loom_u8 loom_actor_health(loom_u8 index)
{
    return index < loom_actor_pool.count ? loom_actor_pool.health[index] : 0u;
}

loom_u8 loom_actor_damage(loom_u8 index, loom_u8 amount)
{
    if (index >= loom_actor_pool.count ||
        loom_actor_pool.alive[index] == LOOM_FALSE ||
        loom_actor_pool.health[index] == 0u || amount == 0u) {
        return LOOM_FALSE;
    }
    if (loom_actor_pool.health[index] > amount) {
        loom_actor_pool.health[index] =
            (loom_u8)(loom_actor_pool.health[index] - amount);
        ++loom_actor_debug_epoch;
        return LOOM_FALSE;
    }
    loom_actor_pool.health[index] = 0u;
    loom_actor_pool.alive[index] = LOOM_FALSE;
    if (loom_actor_type_at(index)->collision == LOOM_ACTOR_COLLISION_SOLID) {
        loom_actor_solid_remove(index);
    }
    /* A dead actor leaves the screen the way a gated one does. */
    (void)loom_mode1_set_sprite_visible(loom_actor_pool.sprite_slot[index],
                                        LOOM_FALSE);
    ++loom_actor_debug_epoch;
    return LOOM_TRUE;
}

void loom_actor_debug_snapshot(LoomActorDebugSnapshot *snapshot)
{
    loom_u8 index;
    loom_u8 blocked;
    loom_u8 alive;

    blocked = 0u;
    alive = 0u;
    for (index = 0u; index < loom_actor_pool.count; ++index) {
        if (loom_actor_pool.alive[index] == LOOM_FALSE) {
            continue;
        }
        ++alive;
        if ((loom_actor_pool.blocked[index] & 0x03u) != 0u) {
            ++blocked;
        }
    }
    snapshot->count = loom_actor_pool.count;
    snapshot->blocked_count = blocked;
    snapshot->first_x = loom_actor_pool.count != 0u ? loom_actor_pool.x[0] : 0;
    snapshot->first_y = loom_actor_pool.count != 0u ? loom_actor_pool.y[0] : 0;
    snapshot->alive_count = alive;
    snapshot->first_health =
        loom_actor_pool.count != 0u ? loom_actor_pool.health[0] : 0u;
}
