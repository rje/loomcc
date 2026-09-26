/* player_tick: the Loom types the player's platformer tick reads, copied
 * from runtime/include/loom/{types,input,movement}.h and
 * runtime/src/movement.c (LoomMovementState) at Loom 925fc3a^. */
#ifndef PLAYER_TICK_LOOM_MOVEMENT_H
#define PLAYER_TICK_LOOM_MOVEMENT_H

typedef signed char loom_s8;
typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)
#define LOOM_STATIC_ASSERT(name, condition) \
    typedef char loom_static_assert_##name[(condition) ? 1 : -1]

#define LOOM_MOVEMENT_TILE_PIXELS ((loom_u16)16u)
#define LOOM_MOVEMENT_MAX_COLLISION_ROWS ((loom_u16)64u)
#define LOOM_MOVEMENT_COLLISION_NONE ((loom_u8)0u)
#define LOOM_MOVEMENT_COLLISION_SOLID ((loom_u8)1u)
#define LOOM_MOVEMENT_COLLISION_ONE_WAY ((loom_u8)2u)
#define LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ((loom_u8)3u)
#define LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT ((loom_u8)4u)
#define LOOM_PLATFORMER_FLAG_TURN_AT_LEDGES ((loom_u8)0x01u)

#define LOOM_PLATFORMER_FLAG_RUN ((loom_u8)0x02u)
#define LOOM_MOVEMENT_WALL_LEFT ((loom_u8)0x01u)
#define LOOM_MOVEMENT_WALL_RIGHT ((loom_u8)0x02u)
#define LOOM_ACTOR_INVALID_INDEX ((loom_u8)0xffu)

#define LOOM_BUTTON_B ((loom_u16)0x8000u)
#define LOOM_BUTTON_Y ((loom_u16)0x4000u)
#define LOOM_BUTTON_LEFT ((loom_u16)0x0200u)
#define LOOM_BUTTON_RIGHT ((loom_u16)0x0100u)
#define LOOM_BUTTON_X ((loom_u16)0x0040u)

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
    loom_u16 run_speed;
    loom_u16 dash_speed;
    loom_u16 skid;
    loom_u16 jump_speed_fast;
    loom_u16 gravity_hold;
    loom_u8 dash_ticks;
    loom_u8 reserved2;
} LoomPlatformerBody;

typedef struct LoomMovementGrid {
    const loom_u8 *cells;
    loom_s16 pixel_width;
    loom_s16 pixel_height;
    loom_u16 row_offsets[LOOM_MOVEMENT_MAX_COLLISION_ROWS];
} LoomMovementGrid;
extern LoomMovementGrid loom_movement_grid;

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
    loom_u8 body;
    const loom_u8 *collision_cells;
    const LoomPlatformerBody *platformer;
    loom_u8 static_collider_count;
    const LoomStaticCollider *static_colliders;
} LoomMovementScene;

/* runtime/src/movement.c; exported so body.asm can read it by offset. */
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
    loom_s16 velocity_x;
    loom_s16 velocity_y;
    loom_u8 on_ground;
    loom_u8 coyote_left;
    loom_u8 buffer_left;
    loom_u8 jumping;
    loom_u8 wall;
    loom_u8 riding;
    loom_u8 landed;
    loom_u8 view_enabled;
    loom_u8 dash_meter;
    loom_s16 view_left;
    loom_s16 view_top;
    loom_s16 view_right;
    loom_s16 view_bottom;
    const LoomMovementScene *scene;
} LoomMovementState;
extern LoomMovementState loom_movement_state;

#if defined(__65816__)
/* The layouts body.asm reads by offset (asserted in movement.c at 925fc3a). */
LOOM_STATIC_ASSERT(movement_state_bytes, sizeof(LoomMovementState) == 40u);
LOOM_STATIC_ASSERT(movement_scene_bytes, sizeof(LoomMovementScene) == 40u);
LOOM_STATIC_ASSERT(platformer_body_bytes, sizeof(LoomPlatformerBody) == 32u);
LOOM_STATIC_ASSERT(movement_grid_bytes, sizeof(LoomMovementGrid) == 8u + 2u * 64u);
#endif

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

/* `whole`/`sub` advance by `velocity` (signed 8.8 subpixels). */
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

/* The generated actor hooks the C tick asks (driver.c: a project without
 * placed actors -- body.asm's tick runs only while no solid actor exists). */
loom_u8 loom_generated_actor_blocks_box(loom_s16 left, loom_s16 top,
                                        loom_s16 right, loom_s16 bottom);
loom_u8 loom_generated_actor_floor_below(loom_s16 left, loom_s16 right,
                                         loom_s16 top, loom_s16 bottom,
                                         loom_s16 *floor);

void loom_pvs_player_tick(loom_u16 held, loom_u16 pressed);

#endif
