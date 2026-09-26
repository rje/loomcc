#include <loom/movement.h>

#define LOOM_MOVEMENT_MAX_SCENE_PIXELS ((loom_u16)32000u)
#define LOOM_MOVEMENT_MAX_COLLIDER_PIXELS ((loom_u16)255u)

typedef struct LoomMovementState {
    loom_s16 player_x;
    loom_s16 player_y;
    loom_u8 subpixel_x;
    loom_u8 subpixel_y;
    loom_u16 blocked_count;
    loom_u8 last_collision;
    loom_u8 initialized;
    loom_u8 moving;
    loom_s8 facing_x;
    loom_s8 facing_y;
    /* The platformer body: signed 8.8 velocities (y grows downward), and
     * the ground and jump state the next tick reads. */
    loom_s16 velocity_x;
    loom_s16 velocity_y;
    loom_u8 on_ground;
    loom_u8 coyote_left;
    loom_u8 buffer_left;
    loom_u8 jumping;
    loom_u8 wall;
    /* The solid actor the platformer body stands on, or 0xff. */
    loom_u8 riding;
    /* Set on the tick the body lands, for the landing clip. */
    loom_u8 landed;
    /* The box the player may not leave while a co-op camera frames two. */
    loom_u8 view_enabled;
    /* Ticks spent at run speed, toward the dash (PHY-002). */
    loom_u8 dash_meter;
    loom_s16 view_left;
    loom_s16 view_top;
    loom_s16 view_right;
    loom_s16 view_bottom;
    const LoomMovementScene *scene;
} LoomMovementState;

/* Exported, not static: a debug ROM's tests read the body's velocities and
 * dash meter through it by symbol and offset, the way the Game tab's
 * watches do. */
LoomMovementState loom_movement_state;
LoomMovementGrid loom_movement_grid;
/* Where the top-down tick last put the player's sprite: most ticks it has
 * not moved, and the Mode 1 call costs more than the rest of an idle tick.
 * Every other place that sets the sprite clears sent_valid. */
static loom_s16 loom_movement_sent_x;
static loom_s16 loom_movement_sent_y;
static loom_u8 loom_movement_sent_valid;
loom_u8 loom_movement_solid_actors;
loom_u8 loom_movement_static_colliders;
loom_s16 loom_movement_probe_floor;

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
#if defined(__65816__)
/* body.asm reads the grid and the state by offset; the host's wider
 * pointers make the same checks meaningless there. */
LOOM_STATIC_ASSERT(loom_movement_grid_matches_body_asm,
                   sizeof(LoomMovementGrid) == LOOM_MOVEMENT_GRID_BYTES);
LOOM_STATIC_ASSERT(loom_movement_state_matches_body_asm,
                   sizeof(LoomMovementState) == LOOM_MOVEMENT_STATE_BYTES);
#endif

static LoomStatus loom_movement_validate_scene(const LoomMovementScene *scene)
{
    loom_u16 cell_count;

    if (scene->pixel_width == 0u || scene->pixel_height == 0u ||
        scene->pixel_width > LOOM_MOVEMENT_MAX_SCENE_PIXELS ||
        scene->pixel_height > LOOM_MOVEMENT_MAX_SCENE_PIXELS ||
        scene->collider_x < (loom_s16)-255 || scene->collider_x > 255 ||
        scene->collider_y < (loom_s16)-255 || scene->collider_y > 255 ||
        scene->collider_width == 0u || scene->collider_height == 0u ||
        scene->collider_width > LOOM_MOVEMENT_MAX_COLLIDER_PIXELS ||
        scene->collider_height > LOOM_MOVEMENT_MAX_COLLIDER_PIXELS ||
        scene->speed_subpixels_per_tick == 0u ||
        scene->speed_subpixels_per_tick >
            LOOM_MOVEMENT_MAX_SPEED_SUBPIXELS ||
        scene->collision_width == 0u || scene->collision_height == 0u ||
        scene->collision_width > LOOM_MOVEMENT_MAX_COLLISION_CELLS ||
        scene->collision_height > LOOM_MOVEMENT_MAX_COLLISION_ROWS ||
        scene->pixel_width % LOOM_MOVEMENT_TILE_PIXELS != 0u ||
        scene->pixel_height % LOOM_MOVEMENT_TILE_PIXELS != 0u ||
        scene->pixel_width / LOOM_MOVEMENT_TILE_PIXELS !=
            scene->collision_width ||
        scene->pixel_height / LOOM_MOVEMENT_TILE_PIXELS !=
            scene->collision_height ||
        scene->body == LOOM_MOVEMENT_BODY_NONE ||
        scene->body > LOOM_MOVEMENT_BODY_PLATFORMER ||
        (scene->body == LOOM_MOVEMENT_BODY_PLATFORMER) !=
            (scene->platformer != (const LoomPlatformerBody *)0) ||
        scene->collision_cells == (const loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (scene->collision_width >
        (loom_u16)(LOOM_MOVEMENT_MAX_COLLISION_CELLS /
                   scene->collision_height)) {
        return LOOM_STATUS_CAPACITY;
    }
    cell_count =
        (loom_u16)(scene->collision_width * scene->collision_height);
    if (cell_count > LOOM_MOVEMENT_MAX_COLLISION_CELLS) {
        return LOOM_STATUS_CAPACITY;
    }
    return LOOM_STATUS_OK;
}

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
/* runtime/backends/pvsneslib/src/movement.asm; the C rendition follows the
 * `#else` below and is what the host suites exercise. The scene's fields are
 * passed by value rather than as a struct pointer, so the assembly needs no
 * knowledge of LoomMovementScene's layout and cannot drift from it. */
#define LOOM_MOVEMENT_BOX_BLOCKED_FAST 1
loom_u8 loom_pvs_movement_box_blocked(const loom_u8 *cells,
                                      loom_u16 collision_width,
                                      loom_u16 pixel_width,
                                      loom_u16 pixel_height,
                                      loom_s16 left,
                                      loom_s16 top,
                                      loom_s16 right,
                                      loom_s16 bottom);
#endif

loom_u8 loom_movement_box_blocked(loom_s16 position_x,
                                  loom_u8 subpixel_x,
                                  loom_s16 position_y,
                                  loom_u8 subpixel_y,
                                  loom_s16 collider_x,
                                  loom_s16 collider_y,
                                  loom_u16 collider_width,
                                  loom_u16 collider_height)
{
    const LoomMovementScene *scene;
    loom_s16 left;
    loom_s16 top;
    loom_s16 right;
    loom_s16 bottom;
#if !defined(LOOM_MOVEMENT_BOX_BLOCKED_FAST)
    loom_u16 first_x;
    loom_u16 last_x;
    loom_u16 first_y;
    loom_u16 last_y;
    loom_u16 x;
    loom_u16 y;
#endif

    scene = loom_movement_state.scene;
    if (scene == (const LoomMovementScene *)0) {
        return LOOM_MOVEMENT_COLLISION_NONE;
    }
    left = (loom_s16)(position_x + collider_x);
    top = (loom_s16)(position_y + collider_y);
    right = (loom_s16)(left + (loom_s16)collider_width - 1);
    bottom = (loom_s16)(top + (loom_s16)collider_height - 1);
    if (subpixel_x != 0u) {
        ++right;
    }
    if (subpixel_y != 0u) {
        ++bottom;
    }
#if defined(LOOM_MOVEMENT_BOX_BLOCKED_FAST)
    if (loom_pvs_movement_box_blocked(scene->collision_cells,
                                      scene->collision_width,
                                      scene->pixel_width, scene->pixel_height,
                                      left, top, right, bottom) !=
        LOOM_MOVEMENT_COLLISION_NONE) {
        return LOOM_MOVEMENT_COLLISION_SOLID;
    }
#else
    if (left < 0 || top < 0 || right >= (loom_s16)scene->pixel_width ||
        bottom >= (loom_s16)scene->pixel_height) {
        return LOOM_MOVEMENT_COLLISION_SOLID;
    }

    first_x = (loom_u16)left / LOOM_MOVEMENT_TILE_PIXELS;
    last_x = (loom_u16)right / LOOM_MOVEMENT_TILE_PIXELS;
    first_y = (loom_u16)top / LOOM_MOVEMENT_TILE_PIXELS;
    last_y = (loom_u16)bottom / LOOM_MOVEMENT_TILE_PIXELS;
    for (y = first_y; y <= last_y; ++y) {
        const loom_u8 *row;

        /* One multiply per row: 816-tcc calls a helper for each one. */
        row = scene->collision_cells + (loom_u16)(y * scene->collision_width);
        for (x = first_x; x <= last_x; ++x) {
            if (row[x] != LOOM_MOVEMENT_COLLISION_NONE) {
                return LOOM_MOVEMENT_COLLISION_SOLID;
            }
        }
    }
#endif
    /* A static collider blocks a body the way a wall does, at any pixel
     * (PHY-002). A scene without any pays one byte test here. */
    if (scene->static_collider_count != 0u) {
        const LoomStaticCollider *collider;
        loom_u8 index;

        collider = scene->static_colliders;
        for (index = 0u; index < scene->static_collider_count; ++index) {
            if (left <= collider->right && right >= collider->left &&
                top <= collider->bottom && bottom >= collider->top) {
                return LOOM_MOVEMENT_COLLISION_SOLID;
            }
            ++collider;
        }
    }
    /* A solid actor blocks a body the way a wall does. The generated bridge
     * answers false when the project places no actors. */
    if (loom_generated_actor_blocks_box(left, top, right, bottom) !=
        LOOM_FALSE) {
        return LOOM_MOVEMENT_COLLISION_SOLID;
    }
    return LOOM_MOVEMENT_COLLISION_NONE;
}

static loom_u8 loom_movement_collision_at(loom_s16 player_x,
                                          loom_u8 subpixel_x,
                                          loom_s16 player_y,
                                          loom_u8 subpixel_y)
{
    const LoomMovementScene *scene;

    scene = loom_movement_state.scene;
    return loom_movement_box_blocked(player_x, subpixel_x, player_y,
                                     subpixel_y, scene->collider_x,
                                     scene->collider_y, scene->collider_width,
                                     scene->collider_height);
}

void loom_movement_step(loom_s16 current,
                        loom_u8 subpixel,
                        loom_s8 direction,
                        loom_u16 speed_subpixels_per_tick,
                        loom_s16 *next,
                        loom_u8 *next_subpixel)
{
    loom_u16 whole;
    loom_u16 fraction;
    loom_u16 combined;
    loom_u16 delta;

    if (direction == 0) {
        *next = current;
        *next_subpixel = subpixel;
        return;
    }
    whole = (loom_u16)(speed_subpixels_per_tick >> LOOM_MOVEMENT_SUBPIXEL_BITS);
    fraction = (loom_u16)(speed_subpixels_per_tick & 0x00ffu);
    if (direction > 0) {
        combined = (loom_u16)subpixel + fraction;
        delta = (loom_u16)(whole + (combined >> LOOM_MOVEMENT_SUBPIXEL_BITS));
        *next = (loom_s16)(current + (loom_s16)delta);
        *next_subpixel = (loom_u8)(combined & 0x00ffu);
    } else {
        loom_u16 borrow;

        borrow = (loom_u16)(subpixel < fraction);
        delta = (loom_u16)(whole + borrow);
        *next = (loom_s16)(current - (loom_s16)delta);
        *next_subpixel = (loom_u8)(((loom_u16)subpixel - fraction) & 0x00ffu);
    }
}

static void loom_movement_candidate(loom_s16 current,
                                    loom_u8 subpixel,
                                    loom_s8 direction,
                                    loom_s16 *next,
                                    loom_u8 *next_subpixel)
{
    loom_movement_step(current, subpixel, direction,
                       loom_movement_state.scene->speed_subpixels_per_tick,
                       next, next_subpixel);
}

/* Debug witness change tracking: steps whenever a snapshot field changes. */
loom_u8 loom_movement_debug_epoch;

/* The body at rest at its current position: no velocity, in the air until
 * the next tick finds the floor, no jump in flight or buffered. */
static void loom_movement_rest_body(void)
{
    loom_movement_state.subpixel_x = 0u;
    loom_movement_state.subpixel_y = 0u;
    loom_movement_state.velocity_x = 0;
    loom_movement_state.velocity_y = 0;
    loom_movement_state.on_ground = LOOM_FALSE;
    loom_movement_state.coyote_left = 0u;
    loom_movement_state.buffer_left = 0u;
    loom_movement_state.jumping = LOOM_FALSE;
    loom_movement_state.wall = 0u;
    loom_movement_state.riding = 0xffu;
    loom_movement_state.landed = LOOM_FALSE;
    loom_movement_state.dash_meter = 0u;
}

LoomStatus loom_movement_initialize(void)
{
    LoomStatus status;

    ++loom_movement_debug_epoch;
    loom_movement_state.player_x = 0;
    loom_movement_state.player_y = 0;
    loom_movement_state.subpixel_x = 0u;
    loom_movement_state.subpixel_y = 0u;
    loom_movement_state.blocked_count = 0u;
    loom_movement_state.velocity_x = 0;
    loom_movement_state.velocity_y = 0;
    loom_movement_state.on_ground = LOOM_FALSE;
    loom_movement_state.coyote_left = 0u;
    loom_movement_state.buffer_left = 0u;
    loom_movement_state.jumping = LOOM_FALSE;
    loom_movement_state.wall = 0u;
    loom_movement_state.riding = 0xffu;
    loom_movement_state.landed = LOOM_FALSE;
    loom_movement_state.moving = LOOM_FALSE;
    loom_movement_state.last_collision = LOOM_MOVEMENT_COLLISION_NONE;
    loom_movement_state.scene = (const LoomMovementScene *)0;
    loom_movement_static_colliders = 0u;
    loom_movement_state.initialized = LOOM_TRUE;
    if (loom_generated_movement_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    status = loom_movement_activate_scene(
        &loom_generated_movement_initial_scene,
        loom_generated_movement_initial_scene.initial_player_x,
        loom_generated_movement_initial_scene.initial_player_y);
    if (status != LOOM_STATUS_OK) {
        loom_movement_state.initialized = LOOM_FALSE;
    }
    return status;
}

LoomStatus loom_movement_activate_scene(const LoomMovementScene *scene,
                                         loom_s16 spawn_x,
                                         loom_s16 spawn_y)
{
    const LoomMovementScene *previous_scene;
    LoomStatus status;

    if (loom_movement_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (scene == (const LoomMovementScene *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (scene->static_collider_count > LOOM_MOVEMENT_STATIC_COLLIDERS_MAX ||
        (scene->static_collider_count != 0u &&
         scene->static_colliders == (const LoomStaticCollider *)0)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    status = loom_movement_validate_scene(scene);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    previous_scene = loom_movement_state.scene;
    loom_movement_state.scene = scene;
    {
        loom_u16 row;
        loom_u16 offset;

        loom_movement_grid.cells = scene->collision_cells;
        loom_movement_grid.pixel_width = (loom_s16)scene->pixel_width;
        loom_movement_grid.pixel_height = (loom_s16)scene->pixel_height;
        offset = 0u;
        for (row = 0u; row < scene->collision_height; ++row) {
            loom_movement_grid.row_offsets[row] = offset;
            offset = (loom_u16)(offset + scene->collision_width);
        }
    }
    if (loom_movement_collision_at(spawn_x, 0u, spawn_y, 0u) !=
        LOOM_MOVEMENT_COLLISION_NONE) {
        loom_movement_state.scene = previous_scene;
        if (previous_scene != (const LoomMovementScene *)0) {
            loom_u16 row;
            loom_u16 offset;

            loom_movement_grid.cells = previous_scene->collision_cells;
            loom_movement_grid.pixel_width = (loom_s16)previous_scene->pixel_width;
            loom_movement_grid.pixel_height = (loom_s16)previous_scene->pixel_height;
            offset = 0u;
            for (row = 0u; row < previous_scene->collision_height; ++row) {
                loom_movement_grid.row_offsets[row] = offset;
                offset = (loom_u16)(offset + previous_scene->collision_width);
            }
        }
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    loom_movement_sent_valid = LOOM_FALSE;
    status = loom_mode1_set_sprite_position(
        scene->player_slot, spawn_x, spawn_y);
    if (status != LOOM_STATUS_OK) {
        loom_movement_state.scene = previous_scene;
        return status;
    }
    loom_movement_static_colliders = scene->static_collider_count;
    loom_movement_state.player_x = spawn_x;
    loom_movement_state.player_y = spawn_y;
    loom_movement_state.view_enabled = LOOM_FALSE;
    ++loom_movement_debug_epoch;
    /* The body arrives at rest: a scene entered from a walk kept the walk's
     * velocity, and a platformer body coasted from its spawn until it
     * landed (there is no friction in the air). */
    loom_movement_rest_body();
    loom_movement_state.last_collision = LOOM_MOVEMENT_COLLISION_NONE;
    return LOOM_STATUS_OK;
}

/*
 * The platformer body (PHY-001). Positions are whole pixels plus a subpixel
 * byte; velocities are signed 8.8 subpixels per tick. The collider's bottom
 * row is the feet; its bottom-center pixel is the ground sensor that reads
 * slopes. X resolves first against solid cells only (slopes and one-way
 * planks are floors, never walls), then Y: falling lands on the first solid,
 * one-way or slope surface between the old feet and the new, rising stops
 * under the first solid ceiling. A body on the ground is then snapped to
 * the floor under its sensor, so it walks down slopes without leaving them.
 */

/* The cell at a pixel, or solid outside the room. */
loom_u8 loom_movement_cell_at(loom_s16 x, loom_s16 y)
{
    loom_u8 cell;

    LOOM_MOVEMENT_CELL_AT(cell, x, y);
    return cell;
}

/* The feet row a body would stand on within `cell` at column pixel `x`,
 * for a floor cell (solid, one-way or slope) whose row starts at `top`. */
loom_s16 loom_movement_floor_in_cell(loom_u8 cell, loom_s16 x, loom_s16 top)
{
    loom_s16 within;

    within = (loom_s16)(x & (LOOM_MOVEMENT_TILE_PIXELS - 1));
    if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT) {
        return (loom_s16)(top + (LOOM_MOVEMENT_TILE_PIXELS - 1) - within);
    }
    if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
        return (loom_s16)(top + within);
    }
    return (loom_s16)(top - 1);
}

/* The feet row a slope gives a ground sensor at column `x` whose feet are
 * at row `feet`: the slope cell the sensor is in, or -- when the sensor's
 * cell is solid, which only happens climbing into the next slope tile --
 * the slope in the row above. Returns LOOM_FALSE when no slope applies. */
static loom_u8 loom_movement_slope_feet(loom_s16 x, loom_s16 feet, loom_s16 *floor)
{
    loom_u8 cell;
    loom_s16 row_top;

    row_top = (loom_s16)(feet & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
    LOOM_MOVEMENT_CELL_AT(cell, x, feet);
    if (cell == LOOM_MOVEMENT_COLLISION_SOLID) {
        row_top = (loom_s16)(row_top - LOOM_MOVEMENT_TILE_PIXELS);
        LOOM_MOVEMENT_CELL_AT(cell, x, row_top);
    }
    if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ||
        cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
        *floor = loom_movement_floor_in_cell(cell, x, row_top);
        return LOOM_TRUE;
    }
    return LOOM_FALSE;
}

/* True when any cell of the rows `top..bottom` in column `x` is solid. */
loom_u8 loom_movement_column_solid(loom_s16 x, loom_s16 top, loom_s16 bottom)
{
    loom_s16 y;
    loom_u8 cell;

    for (y = top; y <= bottom; y = (loom_s16)((y | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
        LOOM_MOVEMENT_CELL_AT(cell, x, y);
        if (cell == LOOM_MOVEMENT_COLLISION_SOLID) {
            return LOOM_TRUE;
        }
    }
    return LOOM_FALSE;
}

/* Signed 8.8 velocity applied to a whole+subpixel coordinate. */
void loom_movement_advance(loom_s16 *whole, loom_u8 *sub, loom_s16 velocity)
{
    loom_s8 direction;
    loom_u16 magnitude;

    if (velocity == 0) {
        return;
    }
    direction = velocity < 0 ? (loom_s8)-1 : (loom_s8)1;
    magnitude = velocity < 0 ? (loom_u16)(-velocity) : (loom_u16)velocity;
    loom_movement_step(*whole, *sub, direction, magnitude, whole, sub);
}

static void loom_movement_platformer_tick(const LoomInputSnapshot *input)
{
    const LoomMovementScene *scene;
    const LoomPlatformerBody *body;
    loom_u16 held;
    loom_u16 pressed;
    loom_s8 intent;
    loom_s16 velocity;
    loom_s16 left;
    loom_s16 right;
    loom_s16 top;
    loom_s16 bottom;
    loom_s16 sensor_x;
    loom_s16 next_x;
    loom_u8 next_sub_x;
    loom_s16 next_y;
    loom_u8 next_sub_y;
    loom_u8 grounded;
    loom_u8 jump;
    loom_u8 cell;
    loom_u8 running;
    loom_u8 extended;
    loom_s16 cap;
    loom_s16 speed;

    scene = loom_movement_state.scene;
    body = scene->platformer;
    held = input->pad_count != 0u ? input->pads[0].held : 0u;
    pressed = input->pad_count != 0u ? input->pads[0].pressed : 0u;
    intent = (loom_s8)(((held & LOOM_BUTTON_RIGHT) != 0u) -
                       ((held & LOOM_BUTTON_LEFT) != 0u));
    grounded = loom_movement_state.on_ground;

    /* The speed cap this tick: walking, running with Y or X held, or the
     * dash once the run has lasted dash_ticks on the ground (PHY-002). The
     * meter holds in the air while the run button stays down, so a jump
     * keeps the dash, and drains a tick at a time otherwise. */
    velocity = loom_movement_state.velocity_x;
    extended = (loom_u8)((body->flags & LOOM_PLATFORMER_FLAG_RUN) != 0u);
    speed = 0;
    running = LOOM_FALSE;
    cap = (loom_s16)body->max_speed;
    if (extended != LOOM_FALSE) {
        speed = velocity < 0 ? (loom_s16)-velocity : velocity;
        running = (loom_u8)(body->run_speed != 0u &&
                            (held & (LOOM_BUTTON_Y | LOOM_BUTTON_X)) != 0u);
        if (running != LOOM_FALSE) {
            cap = (loom_s16)body->run_speed;
        }
    }
    if (extended != LOOM_FALSE && body->dash_speed != 0u && body->dash_ticks != 0u) {
        if (running != LOOM_FALSE && grounded != LOOM_FALSE &&
            speed >= (loom_s16)(body->run_speed - body->acceleration)) {
            if (loom_movement_state.dash_meter < body->dash_ticks) {
                ++loom_movement_state.dash_meter;
            }
        } else if (running == LOOM_FALSE || grounded != LOOM_FALSE) {
            if (loom_movement_state.dash_meter != 0u) {
                --loom_movement_state.dash_meter;
            }
        }
        if (loom_movement_state.dash_meter >= body->dash_ticks) {
            cap = (loom_s16)body->dash_speed;
        }
    }

    /* Horizontal: accelerate toward the intent (a skid against the run on
     * the ground), ease back under a cap that dropped, or slow on the
     * ground. */
    if (intent != 0) {
        loom_s16 gain;

        if (grounded == LOOM_FALSE) {
            gain = (loom_s16)body->air_control;
        } else if (extended != LOOM_FALSE && body->skid != 0u &&
                   ((intent > 0 && velocity < 0) || (intent < 0 && velocity > 0))) {
            gain = (loom_s16)body->skid;
        } else {
            gain = (loom_s16)body->acceleration;
        }
        velocity = (loom_s16)(velocity + (intent > 0 ? gain : (loom_s16)-gain));
        if (extended == LOOM_FALSE || speed <= cap) {
            /* Accelerating into the cap stops at it. */
            if (velocity > cap) {
                velocity = cap;
            } else if (velocity < (loom_s16)-cap) {
                velocity = (loom_s16)-cap;
            }
        } else if (velocity > cap) {
            /* Already over a cap that dropped (the run released): ease
             * back by friction, never past the cap; a frictionless body
             * snaps. */
            velocity = (body->friction != 0u &&
                        (loom_s16)(speed - (loom_s16)body->friction) > cap)
                           ? (loom_s16)(speed - (loom_s16)body->friction)
                           : cap;
        } else if (velocity < (loom_s16)-cap) {
            velocity = (body->friction != 0u &&
                        (loom_s16)(speed - (loom_s16)body->friction) > cap)
                           ? (loom_s16)-(loom_s16)(speed - (loom_s16)body->friction)
                           : (loom_s16)-cap;
        }
        loom_movement_state.facing_x = intent;
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
    loom_movement_state.velocity_x = velocity;

    /* Jump: on a press, or a press buffered within buffer_ticks, while on
     * the ground or within coyote_ticks of leaving it. */
    if ((pressed & LOOM_BUTTON_B) != 0u) {
        loom_movement_state.buffer_left = (loom_u8)(body->buffer_ticks + 1u);
    }
    jump = LOOM_FALSE;
    if (loom_movement_state.buffer_left != 0u &&
        (grounded != LOOM_FALSE || loom_movement_state.coyote_left != 0u)) {
        loom_s16 jump_speed;

        /* The jump grows with the run: jump_speed at rest, jump_speed_fast
         * at the fastest cap the body has. */
        jump_speed = (loom_s16)body->jump_speed;
        if (extended != LOOM_FALSE && body->jump_speed_fast != 0u) {
            loom_s16 top;

            top = (loom_s16)(body->dash_speed != 0u   ? body->dash_speed
                             : body->run_speed != 0u ? body->run_speed
                                                     : body->max_speed);
            if (speed > top) {
                speed = top;
            }
            /* Sixteen-bit only: the console's compiler has no 32-bit
             * multiply worth trusting. Both factors drop four bits (they
             * are at most 4096 and 2048), the quotient gets them back. */
            if (body->jump_speed_fast >= body->jump_speed) {
                loom_u16 bonus;

                bonus = (loom_u16)(((loom_u16)((body->jump_speed_fast - body->jump_speed) >> 4) *
                                    (loom_u16)(speed >> 4)) /
                                   (loom_u16)(top >> 4));
                jump_speed = (loom_s16)(jump_speed + (loom_s16)(bonus << 4));
            } else {
                loom_u16 loss;

                loss = (loom_u16)(((loom_u16)((body->jump_speed - body->jump_speed_fast) >> 4) *
                                   (loom_u16)(speed >> 4)) /
                                  (loom_u16)(top >> 4));
                jump_speed = (loom_s16)(jump_speed - (loom_s16)(loss << 4));
            }
        }
        loom_movement_state.velocity_y = (loom_s16)-jump_speed;
        loom_movement_state.on_ground = LOOM_FALSE;
        loom_movement_state.coyote_left = 0u;
        loom_movement_state.buffer_left = 0u;
        loom_movement_state.jumping = LOOM_TRUE;
        grounded = LOOM_FALSE;
        jump = LOOM_TRUE;
    }
    if (loom_movement_state.buffer_left != 0u) {
        --loom_movement_state.buffer_left;
    }
    if (loom_movement_state.coyote_left != 0u) {
        --loom_movement_state.coyote_left;
    }
    /* Releasing early cuts the rise: the jump's height follows the hold. */
    velocity = loom_movement_state.velocity_y;
    if (loom_movement_state.jumping != LOOM_FALSE) {
        /* A jump_cut at jump_speed means no cut at all, whatever speed the
         * jump left at (a run-and-dash body's grows with the run). */
        if ((held & LOOM_BUTTON_B) == 0u && jump == LOOM_FALSE &&
            body->jump_cut < body->jump_speed &&
            velocity < (loom_s16)-(loom_s16)body->jump_cut) {
            velocity = (loom_s16)-(loom_s16)body->jump_cut;
        }
        if (velocity >= 0) {
            loom_movement_state.jumping = LOOM_FALSE;
        }
    }
    if (grounded == LOOM_FALSE) {
        /* Holding the jump button lightens gravity when the body says so:
         * the float that lets a hold steer a jump's height and a fall. */
        velocity = (loom_s16)(velocity +
                              (loom_s16)((extended != LOOM_FALSE &&
                                          body->gravity_hold != 0u &&
                                          (held & LOOM_BUTTON_B) != 0u)
                                             ? body->gravity_hold
                                             : body->gravity));
        if (velocity > (loom_s16)body->terminal_velocity) {
            velocity = (loom_s16)body->terminal_velocity;
        }
    }
    loom_movement_state.velocity_y = velocity;

    /* The collider in room pixels at the current position. */
    left = (loom_s16)(loom_movement_state.player_x + scene->collider_x);
    top = (loom_s16)(loom_movement_state.player_y + scene->collider_y);
    right = (loom_s16)(left + (loom_s16)scene->collider_width - 1);
    bottom = (loom_s16)(top + (loom_s16)scene->collider_height - 1);

    /* X against solid cells only. A wall stops the body flush against it. */
    loom_movement_state.wall = 0u;
    loom_movement_state.landed = LOOM_FALSE;
    next_x = loom_movement_state.player_x;
    next_sub_x = loom_movement_state.subpixel_x;
    LOOM_MOVEMENT_ADVANCE(next_x, next_sub_x, loom_movement_state.velocity_x);
    if (next_x != loom_movement_state.player_x) {
        loom_s16 delta;
        loom_s16 edge;
        loom_s16 probe;
        loom_s16 stop;

        loom_s16 wall_bottom;

        delta = (loom_s16)(next_x - loom_movement_state.player_x);
        edge = delta > 0 ? right : left;
        stop = next_x;
        /* On the ground the feet's half tile is a step, not a wall: a slope
         * meets its plateau one pixel low, and a lip that high is walked.
         * The step is measured from where the feet will be: a body moving
         * several pixels a tick up a slope would otherwise meet the solid
         * cell under the next slope tile with the feet it had before. */
        wall_bottom = bottom;
        if (grounded != LOOM_FALSE) {
            loom_s16 climb;
            loom_s16 dest_sensor;

            climb = bottom;
            dest_sensor = (loom_s16)(next_x + scene->collider_x +
                                     (loom_s16)(scene->collider_width / 2u));
            if (loom_movement_slope_feet(dest_sensor, bottom, &climb) != LOOM_FALSE &&
                climb > bottom) {
                climb = bottom;
            }
            wall_bottom = (loom_s16)(climb - LOOM_MOVEMENT_TILE_PIXELS / 2);
            if (wall_bottom < top) {
                wall_bottom = top;
            }
        }
#if defined(LOOM_BODY_PROBES_FAST)
        probe = loom_pvs_body_probe_x(edge, delta, top, wall_bottom);
        if (probe != (loom_s16)0x7fff) {
            /* Flush: the edge sits one pixel before the blocking cell. */
            stop = (loom_s16)(loom_movement_state.player_x +
                              (probe - edge) - (delta > 0 ? 1 : -1));
            loom_movement_state.wall =
                delta > 0 ? LOOM_MOVEMENT_WALL_RIGHT : LOOM_MOVEMENT_WALL_LEFT;
            loom_movement_state.velocity_x = 0;
            next_sub_x = 0u;
        }
#else
        for (probe = (loom_s16)(edge + (delta > 0 ? 1 : -1));
             delta > 0 ? probe <= (loom_s16)(right + delta)
                       : probe >= (loom_s16)(left + delta);
             probe = (loom_s16)(probe + (delta > 0 ? 1 : -1))) {
            if (loom_movement_column_solid(probe, top, wall_bottom) != LOOM_FALSE) {
                /* Flush: the edge sits one pixel before the blocking cell. */
                stop = (loom_s16)(loom_movement_state.player_x +
                                  (probe - edge) - (delta > 0 ? 1 : -1));
                loom_movement_state.wall =
                    delta > 0 ? LOOM_MOVEMENT_WALL_RIGHT : LOOM_MOVEMENT_WALL_LEFT;
                loom_movement_state.velocity_x = 0;
                next_sub_x = 0u;
                break;
            }
            /* Cells are 16 wide: after the first probe, step by tiles. */
            if (delta > 0 ? ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) !=
                             LOOM_MOVEMENT_TILE_PIXELS - 1)
                          : ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) != 0)) {
                probe = delta > 0
                            ? (loom_s16)(probe | (LOOM_MOVEMENT_TILE_PIXELS - 1))
                            : (loom_s16)(probe & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
            }
        }
#endif
        /* A solid actor is a wall as well; the body stays where it was. */
        if (stop != loom_movement_state.player_x &&
            loom_generated_actor_blocks_box(
                (loom_s16)(stop + scene->collider_x), top,
                (loom_s16)(stop + scene->collider_x +
                           (loom_s16)scene->collider_width - 1),
                wall_bottom) != LOOM_FALSE) {
            stop = loom_movement_state.player_x;
            loom_movement_state.wall =
                delta > 0 ? LOOM_MOVEMENT_WALL_RIGHT : LOOM_MOVEMENT_WALL_LEFT;
            loom_movement_state.velocity_x = 0;
            next_sub_x = 0u;
        }
        loom_movement_state.player_x = stop;
        loom_movement_state.subpixel_x = next_sub_x;
        left = (loom_s16)(stop + scene->collider_x);
        right = (loom_s16)(left + (loom_s16)scene->collider_width - 1);
    } else {
        loom_movement_state.subpixel_x = next_sub_x;
    }
    sensor_x = (loom_s16)(left + (loom_s16)(scene->collider_width / 2u));

    /* Y. Falling lands on the first floor between the old feet and the
     * new; rising stops under the first solid ceiling. */
    next_y = loom_movement_state.player_y;
    next_sub_y = loom_movement_state.subpixel_y;
    LOOM_MOVEMENT_ADVANCE(next_y, next_sub_y, loom_movement_state.velocity_y);
    if (loom_movement_state.velocity_y > 0 || grounded != LOOM_FALSE) {
        loom_s16 feet;
        loom_s16 reach;
        loom_s16 row_top;
        loom_s16 landed;

        feet = bottom;
        reach = (loom_s16)(next_y + scene->collider_y +
                           (loom_s16)scene->collider_height - 1);
        if (grounded != LOOM_FALSE) {
            /* On the ground the body may step down a slope or a ledge lip:
             * look one tile further so the feet stay on the surface. */
            reach = (loom_s16)(reach + LOOM_MOVEMENT_TILE_PIXELS);
        }
#if defined(LOOM_BODY_PROBES_FAST)
        landed = loom_pvs_body_scan_floor(left, right, feet, reach, sensor_x);
#else
        landed = -1;
        for (row_top = (loom_s16)((feet + 1) & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
             row_top <= reach && landed < 0;
             row_top = (loom_s16)(row_top + LOOM_MOVEMENT_TILE_PIXELS)) {
            loom_s16 x;

            /* The sensor first: it alone reads slopes. */
            LOOM_MOVEMENT_CELL_AT(cell, sensor_x, row_top);
            if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ||
                cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
                loom_s16 floor;

                floor = loom_movement_floor_in_cell(cell, sensor_x, row_top);
                if (floor >= feet - LOOM_MOVEMENT_TILE_PIXELS && floor <= reach) {
                    landed = floor;
                }
                continue;
            }
            for (x = left; x <= right;
                 x = (loom_s16)((x | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
                LOOM_MOVEMENT_CELL_AT(cell, x, row_top);
                if (cell == LOOM_MOVEMENT_COLLISION_SOLID ||
                    (cell == LOOM_MOVEMENT_COLLISION_ONE_WAY && feet < row_top)) {
                    landed = (loom_s16)(row_top - 1);
                    break;
                }
            }
        }
#endif
        /* A solid actor between the old feet and the new is a floor too, and
         * the body rides it from then on. */
        {
            loom_s16 actor_floor;
            loom_u8 platform;

            platform = loom_generated_actor_floor_below(
                left, right, (loom_s16)(feet + 1), reach, &actor_floor);
            if (platform != 0xffu && (landed < 0 || actor_floor < landed)) {
                landed = actor_floor;
                loom_movement_state.riding = platform;
            } else {
                loom_movement_state.riding = 0xffu;
            }
        }
        /* Inside a slope tile already (walking up one), or inside the
         * solid under the next slope tile at speed: the slope under the
         * sensor decides the feet. */
        {
            loom_s16 floor;

            if (loom_movement_slope_feet(sensor_x, feet, &floor) != LOOM_FALSE &&
                (landed < 0 || floor < landed)) {
                landed = floor;
                loom_movement_state.riding = 0xffu;
            }
        }
        if (landed >= 0 && (landed <= reach)) {
            loom_movement_state.player_y =
                (loom_s16)(landed - (loom_s16)scene->collider_height + 1 -
                           scene->collider_y);
            loom_movement_state.subpixel_y = 0u;
            if (grounded == LOOM_FALSE) {
                loom_movement_state.velocity_y = 0;
                loom_movement_state.jumping = LOOM_FALSE;
                loom_movement_state.landed = LOOM_TRUE;
            } else {
                loom_movement_state.velocity_y = 0;
            }
            loom_movement_state.on_ground = LOOM_TRUE;
        } else {
            loom_movement_state.player_y = next_y;
            loom_movement_state.subpixel_y = next_sub_y;
            if (grounded != LOOM_FALSE) {
                /* Walked off an edge: a jump still starts for a few ticks. */
                loom_movement_state.coyote_left = body->coyote_ticks;
            }
            loom_movement_state.on_ground = LOOM_FALSE;
            loom_movement_state.riding = 0xffu;
        }
    } else if (loom_movement_state.velocity_y < 0) {
        loom_s16 head;
        loom_s16 reach;
        loom_s16 row_top;
        loom_s16 stopped;

        loom_movement_state.riding = 0xffu;
        head = top;
        reach = (loom_s16)(next_y + scene->collider_y);
#if defined(LOOM_BODY_PROBES_FAST)
        stopped = loom_pvs_body_scan_ceiling(left, right, head, reach);
#else
        stopped = -1;
        for (row_top = (loom_s16)((head - 1) & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
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
            loom_movement_state.player_y = (loom_s16)(stopped - scene->collider_y);
            loom_movement_state.subpixel_y = 0u;
            loom_movement_state.velocity_y = 0;
            loom_movement_state.jumping = LOOM_FALSE;
        } else {
            loom_movement_state.player_y = next_y;
            loom_movement_state.subpixel_y = next_sub_y;
        }
        loom_movement_state.on_ground = LOOM_FALSE;
    } else {
        loom_movement_state.player_y = next_y;
        loom_movement_state.subpixel_y = next_sub_y;
    }
    loom_movement_state.moving = (loom_u8)(intent != 0);
    if (loom_movement_state.wall != 0u) {
        loom_movement_state.last_collision = LOOM_MOVEMENT_COLLISION_SOLID;
        if (loom_movement_state.blocked_count != 0xffffu) {
            ++loom_movement_state.blocked_count;
        }
    }
}

LoomStatus loom_movement_update(const LoomInputSnapshot *input)
{
    loom_u16 held;
    loom_s8 horizontal;
    loom_s8 vertical;
    loom_u8 blocked;
    LoomStatus status;

    if (input == (const LoomInputSnapshot *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_movement_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_movement_state.scene == (const LoomMovementScene *)0) {
        return LOOM_STATUS_OK;
    }
    if (loom_movement_state.scene->body == LOOM_MOVEMENT_BODY_PLATFORMER) {
#if defined(LOOM_BODY_PROBES_FAST)
        /* The assembly tick places the sprite itself, and asks the pool
         * about solid actors through the probes above when there are any. */
        loom_pvs_player_tick(
            input->pad_count != 0u ? input->pads[0].held : 0u,
            input->pad_count != 0u ? input->pads[0].pressed : 0u);
        ++loom_movement_debug_epoch;
        return LOOM_STATUS_OK;
#endif
        loom_movement_platformer_tick(input);
        ++loom_movement_debug_epoch;
        loom_movement_sent_valid = LOOM_FALSE;
        return loom_mode1_set_sprite_position(
            loom_movement_state.scene->player_slot,
            loom_movement_state.player_x, loom_movement_state.player_y);
    }
    held = input->pad_count != 0u ? input->pads[0].held : 0u;
    horizontal = (loom_s8)(((held & LOOM_BUTTON_RIGHT) != 0u) -
                           ((held & LOOM_BUTTON_LEFT) != 0u));
    vertical = (loom_s8)(((held & LOOM_BUTTON_DOWN) != 0u) -
                         ((held & LOOM_BUTTON_UP) != 0u));
    blocked = LOOM_FALSE;
    if (horizontal != 0 || vertical != 0) {
        loom_movement_state.facing_x = horizontal;
        loom_movement_state.facing_y = vertical;
    }

    if (horizontal != 0) {
        loom_s16 candidate;
        loom_u8 candidate_subpixel;

        loom_movement_candidate(loom_movement_state.player_x,
                                loom_movement_state.subpixel_x, horizontal,
                                &candidate, &candidate_subpixel);
        if (loom_movement_collision_at(
                candidate, candidate_subpixel,
                loom_movement_state.player_y,
                loom_movement_state.subpixel_y) ==
            LOOM_MOVEMENT_COLLISION_NONE) {
            loom_movement_state.player_x = candidate;
            loom_movement_state.subpixel_x = candidate_subpixel;
        } else {
            blocked = LOOM_TRUE;
        }
    }
    if (vertical != 0) {
        loom_s16 candidate;
        loom_u8 candidate_subpixel;

        loom_movement_candidate(loom_movement_state.player_y,
                                loom_movement_state.subpixel_y, vertical,
                                &candidate, &candidate_subpixel);
        if (loom_movement_collision_at(
                loom_movement_state.player_x,
                loom_movement_state.subpixel_x, candidate,
                candidate_subpixel) == LOOM_MOVEMENT_COLLISION_NONE) {
            loom_movement_state.player_y = candidate;
            loom_movement_state.subpixel_y = candidate_subpixel;
        } else {
            blocked = LOOM_TRUE;
        }
    }
    if (blocked != LOOM_FALSE) {
        loom_movement_state.last_collision =
            LOOM_MOVEMENT_COLLISION_SOLID;
        if (loom_movement_state.blocked_count != 0xffffu) {
            ++loom_movement_state.blocked_count;
        }
    }
    loom_movement_state.moving =
        (loom_u8)(horizontal != 0 || vertical != 0);
    if (blocked != LOOM_FALSE || horizontal != 0 || vertical != 0) {
        ++loom_movement_debug_epoch;
    }
    if (loom_movement_state.view_enabled != LOOM_FALSE) {
        /* The view box wins over the step: a player at its edge stays. */
        if (loom_movement_state.player_x < loom_movement_state.view_left) {
            loom_movement_state.player_x = loom_movement_state.view_left;
            loom_movement_state.subpixel_x = 0u;
        } else if (loom_movement_state.player_x >
                   loom_movement_state.view_right) {
            loom_movement_state.player_x = loom_movement_state.view_right;
            loom_movement_state.subpixel_x = 0u;
        }
        if (loom_movement_state.player_y < loom_movement_state.view_top) {
            loom_movement_state.player_y = loom_movement_state.view_top;
            loom_movement_state.subpixel_y = 0u;
        } else if (loom_movement_state.player_y >
                   loom_movement_state.view_bottom) {
            loom_movement_state.player_y = loom_movement_state.view_bottom;
            loom_movement_state.subpixel_y = 0u;
        }
    }
    if (loom_movement_sent_valid != LOOM_FALSE &&
        loom_movement_sent_x == loom_movement_state.player_x &&
        loom_movement_sent_y == loom_movement_state.player_y) {
        return LOOM_STATUS_OK;
    }
    status = loom_mode1_set_sprite_position(
        loom_movement_state.scene->player_slot,
        loom_movement_state.player_x, loom_movement_state.player_y);
    if (status == LOOM_STATUS_OK) {
        loom_movement_sent_x = loom_movement_state.player_x;
        loom_movement_sent_y = loom_movement_state.player_y;
        loom_movement_sent_valid = LOOM_TRUE;
    }
    return status;
}

void loom_movement_set_view_box(loom_s16 left,
                                loom_s16 top,
                                loom_s16 right,
                                loom_s16 bottom,
                                loom_u8 enabled)
{
    loom_movement_state.view_enabled =
        (loom_u8)(enabled != LOOM_FALSE && left <= right && top <= bottom);
    loom_movement_state.view_left = left;
    loom_movement_state.view_top = top;
    loom_movement_state.view_right = right;
    loom_movement_state.view_bottom = bottom;
}

LoomStatus loom_movement_set_player_position(loom_s16 x, loom_s16 y)
{
    LoomStatus status;

    if (loom_movement_state.initialized == LOOM_FALSE ||
        loom_movement_state.scene == (const LoomMovementScene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_movement_collision_at(x, 0u, y, 0u) !=
        LOOM_MOVEMENT_COLLISION_NONE) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    loom_movement_sent_valid = LOOM_FALSE;
    status = loom_mode1_set_sprite_position(
        loom_movement_state.scene->player_slot, x, y);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_movement_state.player_x = x;
    loom_movement_state.player_y = y;
    loom_movement_rest_body();
    /* A spawn relocation or a knockback moves the player without a tick of
     * movement, and a witness that misses it reads as a frozen player. */
    ++loom_movement_debug_epoch;
    return LOOM_STATUS_OK;
}

loom_s16 loom_movement_player_x(void)
{
    return loom_movement_state.player_x;
}

loom_s16 loom_movement_player_y(void)
{
    return loom_movement_state.player_y;
}

loom_u8 loom_movement_player_subpixel_x(void)
{
    return loom_movement_state.subpixel_x;
}

loom_u8 loom_movement_player_subpixel_y(void)
{
    return loom_movement_state.subpixel_y;
}

loom_s8 loom_movement_facing_x(void)
{
    return loom_movement_state.facing_x;
}

loom_s8 loom_movement_facing_y(void)
{
    return loom_movement_state.facing_y;
}

void loom_movement_debug_snapshot(LoomMovementDebugSnapshot *snapshot)
{
    snapshot->player_x = loom_movement_state.player_x;
    snapshot->player_y = loom_movement_state.player_y;
    snapshot->blocked_count = loom_movement_state.blocked_count;
    snapshot->last_collision = loom_movement_state.last_collision;
    snapshot->reserved = 0u;
}

/* Platformer body state for animation, behaviors and tests. */
loom_u8 loom_movement_on_ground(void)
{
    return loom_movement_state.on_ground;
}

loom_s16 loom_movement_velocity_x(void)
{
    return loom_movement_state.velocity_x;
}

loom_s16 loom_movement_velocity_y(void)
{
    return loom_movement_state.velocity_y;
}

loom_u8 loom_movement_wall(void)
{
    return loom_movement_state.wall;
}

loom_u8 loom_movement_air(void)
{
    if (loom_movement_state.scene == (const LoomMovementScene *)0 ||
        loom_movement_state.scene->body != LOOM_MOVEMENT_BODY_PLATFORMER) {
        return LOOM_BODY_AIR_GROUND;
    }
    if (loom_movement_state.landed != LOOM_FALSE) {
        return LOOM_BODY_AIR_LANDED;
    }
    if (loom_movement_state.on_ground == LOOM_FALSE) {
        return loom_movement_state.velocity_y < 0 ? LOOM_BODY_AIR_RISING
                                                  : LOOM_BODY_AIR_FALLING;
    }
    return LOOM_BODY_AIR_GROUND;
}

loom_u8 loom_movement_animation_key(void)
{
    return (loom_u8)((loom_movement_state.moving != LOOM_FALSE ? 1u : 0u) |
                     ((loom_u8)(loom_movement_state.facing_x + 1) << 1) |
                     ((loom_u8)(loom_movement_state.facing_y + 1) << 3) |
                     (loom_u8)(loom_movement_air() << 5));
}

LoomStatus loom_movement_bounce(void)
{
    const LoomMovementScene *scene;

    scene = loom_movement_state.scene;
    if (loom_movement_state.initialized == LOOM_FALSE ||
        scene == (const LoomMovementScene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    if (scene->body != LOOM_MOVEMENT_BODY_PLATFORMER) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_movement_state.velocity_y =
        (loom_s16)-(loom_s16)scene->platformer->jump_speed;
    loom_movement_state.on_ground = LOOM_FALSE;
    loom_movement_state.coyote_left = 0u;
    loom_movement_state.buffer_left = 0u;
    loom_movement_state.jumping = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

loom_u8 loom_movement_riding(void)
{
    return loom_movement_state.riding;
}

void loom_movement_carry(loom_s16 delta_x, loom_s16 delta_y)
{
    if (loom_movement_state.scene == (const LoomMovementScene *)0 ||
        loom_movement_state.riding == 0xffu) {
        return;
    }
    loom_movement_state.player_x =
        (loom_s16)(loom_movement_state.player_x + delta_x);
    loom_movement_state.player_y =
        (loom_s16)(loom_movement_state.player_y + delta_y);
}

loom_u8 loom_movement_moving(void)
{
    return loom_movement_state.moving;
}

LoomStatus loom_movement_player_box(loom_s16 *left,
                                    loom_s16 *top,
                                    loom_s16 *right,
                                    loom_s16 *bottom)
{
    const LoomMovementScene *scene;

    scene = loom_movement_state.scene;
    if (scene == (const LoomMovementScene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    *left = (loom_s16)(loom_movement_state.player_x + scene->collider_x);
    *top = (loom_s16)(loom_movement_state.player_y + scene->collider_y);
    *right = (loom_s16)(*left + (loom_s16)scene->collider_width - 1);
    *bottom = (loom_s16)(*top + (loom_s16)scene->collider_height - 1);
    return LOOM_STATUS_OK;
}

loom_u16 loom_movement_blocked_count(void)
{
    return loom_movement_state.blocked_count;
}

loom_u8 loom_movement_last_collision(void)
{
    return loom_movement_state.last_collision;
}
