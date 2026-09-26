#ifndef LOOM_MOVEMENT_H
#define LOOM_MOVEMENT_H

#include <loom/mode1.h>

#define LOOM_MOVEMENT_SUBPIXEL_BITS ((loom_u8)8u)
#define LOOM_MOVEMENT_SUBPIXELS_PER_PIXEL ((loom_u16)256u)
#define LOOM_MOVEMENT_TILE_PIXELS ((loom_u16)16u)
#define LOOM_MOVEMENT_MAX_SPEED_SUBPIXELS ((loom_u16)4096u)
#define LOOM_MOVEMENT_MAX_COLLISION_CELLS ((loom_u16)16384u)
/* Rows a scene's grid may hold: the runtime keeps a row-start table so a
 * cell lookup is two indexed reads and no multiply. */
#define LOOM_MOVEMENT_MAX_COLLISION_ROWS ((loom_u16)64u)

/* One byte per 16x16 cell of the material grid. A top-down body and the
 * assembly probe treat any nonzero cell as solid; a platformer body reads
 * the meaning: a one-way cell is solid only from above while falling, a
 * slope cell's floor rises across the tile toward the named side. */
#define LOOM_MOVEMENT_COLLISION_NONE ((loom_u8)0u)
#define LOOM_MOVEMENT_COLLISION_SOLID ((loom_u8)1u)
#define LOOM_MOVEMENT_COLLISION_ONE_WAY ((loom_u8)2u)
#define LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ((loom_u8)3u)
#define LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT ((loom_u8)4u)

/* Body kinds, as LOOM_ACTOR_BODY_* numbers them: none, top-down, platformer. */
#define LOOM_MOVEMENT_BODY_NONE ((loom_u8)0u)
#define LOOM_MOVEMENT_BODY_TOP_DOWN ((loom_u8)1u)
#define LOOM_MOVEMENT_BODY_PLATFORMER ((loom_u8)2u)

#define LOOM_PLATFORMER_FLAG_TURN_AT_LEDGES ((loom_u8)0x01u)
/* The body uses the run-and-dash extension (PHY-002); the generator sets
 * it when any of those fields is nonzero, so a body without them runs the
 * classic path's instructions and nothing more. */
#define LOOM_PLATFORMER_FLAG_RUN ((loom_u8)0x02u)

/* Where a body is against gravity, for animation: on a floor, rising,
 * falling, or on the tick it lands. A top-down body is always on the
 * ground. */
#define LOOM_BODY_AIR_GROUND ((loom_u8)0u)
#define LOOM_BODY_AIR_RISING ((loom_u8)1u)
#define LOOM_BODY_AIR_FALLING ((loom_u8)2u)
#define LOOM_BODY_AIR_LANDED ((loom_u8)3u)

/* Which side a platformer body is pressed against, after its last step. */
#define LOOM_MOVEMENT_WALL_LEFT ((loom_u8)0x01u)
#define LOOM_MOVEMENT_WALL_RIGHT ((loom_u8)0x02u)

/* A platformer body's tunables: 8.8 subpixels per tick, or ticks. Generated
 * once per authored body and shared by every actor of the type. */
typedef struct LoomPlatformerBody {
    loom_u16 max_speed;
    loom_u16 acceleration;
    loom_u16 friction;
    loom_u16 air_control;
    loom_u16 gravity;
    loom_u16 terminal_velocity;
    loom_u16 jump_speed;
    loom_u16 jump_cut;
    loom_u8 coyote_ticks;
    loom_u8 buffer_ticks;
    loom_u8 flags;
    loom_u8 reserved;
    /* The run-and-dash extension (PHY-002). All zero keeps the body above:
     * a run button (Y or X) raises the speed cap to run_speed; dash_ticks
     * at that cap raise it again to dash_speed; skid is the ground
     * deceleration against the run's direction; jump_speed_fast is the jump
     * at the fastest cap, scaled down to jump_speed at rest; gravity_hold
     * is the gravity while the jump button is held. */
    loom_u16 run_speed;
    loom_u16 dash_speed;
    loom_u16 skid;
    loom_u16 jump_speed_fast;
    loom_u16 gravity_hold;
    loom_u8 dash_ticks;
    loom_u8 reserved2;
} LoomPlatformerBody;

/* How many static colliders one scene may place. */
#define LOOM_MOVEMENT_STATIC_COLLIDERS_MAX ((loom_u8)32u)

/* A solid rectangle an entity places in the room (PHY-002): inclusive edges
 * in room pixels. A body cannot enter it, the way it cannot enter a solid
 * collision cell, and unlike a cell it need not sit on the 16-pixel grid. */
typedef struct LoomStaticCollider {
    loom_s16 left;
    loom_s16 top;
    loom_s16 right;
    loom_s16 bottom;
} LoomStaticCollider;

typedef struct LoomMovementScene {
    loom_u16 pixel_width;
    loom_u16 pixel_height;
    loom_s16 initial_player_x;
    loom_s16 initial_player_y;
    loom_s16 collider_x;
    loom_s16 collider_y;
    loom_u16 collider_width;
    loom_u16 collider_height;
    loom_u16 speed_subpixels_per_tick;
    loom_u16 collision_width;
    loom_u16 collision_height;
    loom_u8 player_slot;
    /* LOOM_MOVEMENT_BODY_*: how the pad moves the player. */
    loom_u8 body;
    const loom_u8 *collision_cells;
    /* The player's tunables when the body is a platformer, else null. */
    const LoomPlatformerBody *platformer;
    /* The scene's static colliders (PHY-002); null when the count is zero. */
    loom_u8 static_collider_count;
    const LoomStaticCollider *static_colliders;
} LoomMovementScene;

/* Defined by generated mode1_data.c for the selected initial scene. */
extern const loom_u8 loom_generated_movement_enabled;
extern const LoomMovementScene loom_generated_movement_initial_scene;

LoomStatus loom_movement_initialize(void);
LoomStatus loom_movement_activate_scene(const LoomMovementScene *scene,
                                         loom_s16 spawn_x,
                                         loom_s16 spawn_y);
LoomStatus loom_movement_update(const LoomInputSnapshot *input);
LoomStatus loom_movement_set_player_position(loom_s16 x, loom_s16 y);
/* A box the player may not leave (inclusive edges, in room pixels), so a
 * co-op camera never loses a player off the screen; `enabled` LOOM_FALSE
 * lifts it. Activation lifts it too. */
void loom_movement_set_view_box(loom_s16 left,
                                loom_s16 top,
                                loom_s16 right,
                                loom_s16 bottom,
                                loom_u8 enabled);
loom_s16 loom_movement_player_x(void);
loom_s16 loom_movement_player_y(void);
loom_u8 loom_movement_player_subpixel_x(void);
loom_u8 loom_movement_player_subpixel_y(void);
/* The last non-zero D-pad direction per axis (-1, 0, 1); zero until the
 * player first moves after a scene activates. */
loom_s8 loom_movement_facing_x(void);
loom_s8 loom_movement_facing_y(void);
typedef struct LoomMovementDebugSnapshot {
    loom_s16 player_x;
    loom_s16 player_y;
    loom_u16 blocked_count;
    loom_u8 last_collision;
    loom_u8 reserved;
} LoomMovementDebugSnapshot;
void loom_movement_debug_snapshot(LoomMovementDebugSnapshot *snapshot);
/* Steps whenever a snapshot field may have changed, so a witness can skip
 * the snapshot on quiet ticks. */
extern loom_u8 loom_movement_debug_epoch;
/* True while the last tick accepted a direction from the pad, which is what
 * selects a move state over an idle one. */
loom_u8 loom_movement_moving(void);
/* The platformer body after its last step: standing on a floor, its signed
 * 8.8 velocities (y grows downward), and the wall it is pressed against.
 * A top-down body reports grounded, still, and no wall. */
loom_u8 loom_movement_on_ground(void);
loom_s16 loom_movement_velocity_x(void);
loom_s16 loom_movement_velocity_y(void);
loom_u8 loom_movement_wall(void);
/* LOOM_BODY_AIR_*: the player's body against gravity after its last step. */
loom_u8 loom_movement_air(void);
/* The four answers above packed as an animation drive key: moving |
 * (facing_x + 1) << 1 | (facing_y + 1) << 3 | air << 5, in one call. */
loom_u8 loom_movement_animation_key(void);
/* The bounce off a stomped actor: a fresh jump for the platformer body, so
 * releasing the button early cuts it short the same way. Not a platformer:
 * LOOM_STATUS_INVALID_ARGUMENT, and nothing moves. */
LoomStatus loom_movement_bounce(void);

/*
 * True when the scene's material grid or a solid actor blocks this box at
 * this position. The actor bodies resolve against the same grid the player
 * does, so one implementation serves both.
 */
loom_u8 loom_movement_box_blocked(loom_s16 position_x,
                                  loom_u8 subpixel_x,
                                  loom_s16 position_y,
                                  loom_u8 subpixel_y,
                                  loom_s16 collider_x,
                                  loom_s16 collider_y,
                                  loom_u16 collider_width,
                                  loom_u16 collider_height);

/*
 * The platformer body's pure pieces, shared by the player's body here and
 * the actor bodies in actor.c: the cell at a pixel (solid outside the room),
 * the feet row a floor cell offers at a column pixel, whether a column of
 * rows holds a solid cell, and a signed 8.8 velocity applied to a
 * whole-plus-subpixel coordinate.
 */
loom_u8 loom_movement_cell_at(loom_s16 x, loom_s16 y);

/* The active scene's grid, resolved once at activation: the cells, the
 * room's pixel bounds, and where each row starts in the cells, so a lookup
 * never multiplies. Exported so the lookup below can be a statement in the
 * bodies that read it a dozen times a tick -- 816-tcc pays a call and four
 * stack arguments for each call otherwise. */
typedef struct LoomMovementGrid {
    const loom_u8 *cells;
    loom_s16 pixel_width;
    loom_s16 pixel_height;
    loom_u16 row_offsets[LOOM_MOVEMENT_MAX_COLLISION_ROWS];
} LoomMovementGrid;
extern LoomMovementGrid loom_movement_grid;
/* body.asm reads this struct by offset (cells at 0, the bounds at 4 and 6,
 * the row offsets from 8): a field added or moved here has to move there. */
#define LOOM_MOVEMENT_GRID_BYTES (8u + 2u * LOOM_MOVEMENT_MAX_COLLISION_ROWS)

/* body.asm: the platformer bodies' tile probes on the console; the C loops
 * they replace follow each call site under `#else` for the host suites. */
#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
#define LOOM_BODY_PROBES_FAST 1
loom_s16 loom_pvs_body_probe_x(loom_s16 edge, loom_s16 delta, loom_s16 top, loom_s16 wall_bottom);
loom_s16 loom_pvs_body_scan_floor(loom_s16 left, loom_s16 right, loom_s16 feet, loom_s16 reach, loom_s16 sensor_x);
loom_s16 loom_pvs_body_scan_ceiling(loom_s16 left, loom_s16 right, loom_s16 head, loom_s16 reach);
/* The whole player platformer tick on the console (PERF-004), for rooms
 * without solid actors; it reads and writes loom_movement_state by offset. */
void loom_pvs_player_tick(loom_u16 held, loom_u16 pressed);
#endif
/* body.asm reads loom_movement_state by offset: a field added or moved in
 * LoomMovementState has to move there too. */
#define LOOM_MOVEMENT_STATE_BYTES 40u
/* How many solid actors the pool holds, kept by actor.c. The console's
 * player tick runs in body.asm only while it is zero: a solid actor is a
 * wall and a floor that only the C tick handles. */
extern loom_u8 loom_movement_solid_actors;
/* The resident scene's static collider count: the actor pass leaves a
 * top-down actor to the C tick while there are any. */
extern loom_u8 loom_movement_static_colliders;
/* The assembly bodies' questions about solid actors, over the generated
 * actor hooks: the highest solid actor top in the box (its index or 0xff,
 * the feet row in loom_movement_probe_floor), and whether one covers a box. */
extern loom_s16 loom_movement_probe_floor;
loom_u16 loom_movement_actor_floor_probe(loom_s16 left, loom_s16 right,
                                         loom_s16 top, loom_s16 bottom);
loom_u16 loom_movement_actor_block_probe(loom_s16 left, loom_s16 top,
                                         loom_s16 right, loom_s16 bottom);

/* `result` = the cell at pixel (px, py), or solid outside the room. */
#define LOOM_MOVEMENT_CELL_AT(result, px, py)                                    \
    do {                                                                         \
        loom_s16 loom_cell_x_ = (px);                                            \
        loom_s16 loom_cell_y_ = (py);                                            \
        if (loom_cell_x_ < 0 || loom_cell_y_ < 0 ||                              \
            loom_cell_x_ >= loom_movement_grid.pixel_width ||                    \
            loom_cell_y_ >= loom_movement_grid.pixel_height) {                   \
            (result) = LOOM_MOVEMENT_COLLISION_SOLID;                            \
        } else {                                                                 \
            (result) = loom_movement_grid.cells                                  \
                [loom_movement_grid.row_offsets[(loom_u16)loom_cell_y_ >> 4] +  \
                 ((loom_u16)loom_cell_x_ >> 4)];                                 \
        }                                                                        \
    } while (0)

/* `whole`/`sub` advance by `velocity` (signed 8.8 subpixels): the same
 * arithmetic as loom_movement_step, without two calls per axis per tick. */
#define LOOM_MOVEMENT_ADVANCE(whole, sub, velocity)                              \
    do {                                                                         \
        loom_s16 loom_adv_v_ = (velocity);                                       \
        if (loom_adv_v_ > 0) {                                                   \
            loom_u16 loom_adv_c_ =                                               \
                (loom_u16)((loom_u16)(sub) + ((loom_u16)loom_adv_v_ & 0x00ffu)); \
            (whole) = (loom_s16)((whole) +                                       \
                                 (loom_s16)(((loom_u16)loom_adv_v_ >> 8) +       \
                                            (loom_adv_c_ >> 8)));                \
            (sub) = (loom_u8)(loom_adv_c_ & 0x00ffu);                            \
        } else if (loom_adv_v_ < 0) {                                            \
            loom_u16 loom_adv_m_ = (loom_u16)(-loom_adv_v_);                     \
            loom_u16 loom_adv_f_ = (loom_u16)(loom_adv_m_ & 0x00ffu);            \
            loom_u16 loom_adv_b_ = (loom_u16)((loom_u16)(sub) < loom_adv_f_);    \
            (whole) = (loom_s16)((whole) -                                       \
                                 (loom_s16)((loom_adv_m_ >> 8) + loom_adv_b_));  \
            (sub) = (loom_u8)(((loom_u16)(sub) - loom_adv_f_) & 0x00ffu);        \
        }                                                                        \
    } while (0)
loom_s16 loom_movement_floor_in_cell(loom_u8 cell, loom_s16 x, loom_s16 top);
loom_u8 loom_movement_column_solid(loom_s16 x, loom_s16 top, loom_s16 bottom);
void loom_movement_advance(loom_s16 *whole, loom_u8 *sub, loom_s16 velocity);

/*
 * Advances one axis by a signed 8.8 speed, carrying the subpixel remainder.
 * Both the player and the actor bodies step this way.
 */
void loom_movement_step(loom_s16 current,
                        loom_u8 subpixel,
                        loom_s8 direction,
                        loom_u16 speed_subpixels_per_tick,
                        loom_s16 *next,
                        loom_u8 *next_subpixel);

/*
 * Emitted by the generated schedule: true when a solid actor covers the box.
 * A project without actors generates a version that always answers false.
 */
loom_u8 loom_generated_actor_blocks_box(loom_s16 left,
                                        loom_s16 top,
                                        loom_s16 right,
                                        loom_s16 bottom);
/*
 * Emitted by the generated schedule: the highest solid actor box top within
 * rows `top..bottom` under columns `left..right`, as the feet row a body
 * would stand on, and that actor's index -- or LOOM_ACTOR_INVALID_INDEX
 * (0xff) when none. A project without actors generates a version that
 * always answers none.
 */
loom_u8 loom_generated_actor_floor_below(loom_s16 left,
                                         loom_s16 right,
                                         loom_s16 top,
                                         loom_s16 bottom,
                                         loom_s16 *floor);
/* The actor a platformer player stands on, or 0xff. */
loom_u8 loom_movement_riding(void);
/* Moves the player with the platform it stands on, before its own step. */
void loom_movement_carry(loom_s16 delta_x, loom_s16 delta_y);
/*
 * The player's body box in world pixels, which is also its hurt box. Fails
 * when no movement scene is active.
 */
LoomStatus loom_movement_player_box(loom_s16 *left,
                                    loom_s16 *top,
                                    loom_s16 *right,
                                    loom_s16 *bottom);
loom_u16 loom_movement_blocked_count(void);
loom_u8 loom_movement_last_collision(void);

#endif
