/* actor_body_step: the Loom types the actor body step reads, copied from
 * runtime/include/loom/{types,movement,actor}.h at Loom 13d8987 (the record
 * LoomActorBodyStep is new there; LoomPlatformerBody and LoomMovementGrid
 * are unchanged from 13d8987^). */
#ifndef ACTOR_BODY_STEP_LOOM_BODY_H
#define ACTOR_BODY_STEP_LOOM_BODY_H

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

#define LOOM_ACTOR_BODY_FLAG_ON_GROUND ((loom_u8)0x01u)
#define LOOM_ACTOR_BODY_FLAG_WALL_LEFT ((loom_u8)0x02u)
#define LOOM_ACTOR_BODY_FLAG_WALL_RIGHT ((loom_u8)0x04u)
#define LOOM_ACTOR_BODY_FLAG_LANDED ((loom_u8)0x08u)

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

typedef struct LoomActorBodyStep {
    loom_s16 x;
    loom_s16 y;
    loom_s16 vx;
    loom_s16 vy;
    loom_s16 box_x;
    loom_s16 box_y;
    loom_u16 box_w;
    loom_u16 box_h;
    loom_s16 next_y;
    loom_s16 landed;
    loom_s16 stopped;
    loom_s16 reach;
    loom_s16 left;
    loom_s16 right;
    loom_s16 top;
    loom_s16 bottom;
    loom_s16 sensor_x;
    loom_u8 sub_x;
    loom_u8 sub_y;
    loom_u8 next_sub_y;
    loom_s8 intent_x;
    loom_s8 intent_y;
    loom_u8 grounded;
    loom_u8 turn_at_ledges;
    loom_u8 flags;
    loom_u8 jumped;
    loom_u8 phase;
} LoomActorBodyStep;

/* body.asm reads the record, the body and the grid by offset. */
LOOM_STATIC_ASSERT(actor_body_step_bytes, sizeof(LoomActorBodyStep) == 44u);
#if defined(__65816__)
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

/* The tile probes (body.asm at 13d8987^, loom_pvs_body_*): callees of the
 * step, not under test; driver.c provides them in C for both variants. */
loom_s16 loom_pvs_body_probe_x(loom_s16 edge, loom_s16 delta, loom_s16 top, loom_s16 wall_bottom);
loom_s16 loom_pvs_body_scan_floor(loom_s16 left, loom_s16 right, loom_s16 feet, loom_s16 reach, loom_s16 sensor_x);
loom_s16 loom_pvs_body_scan_ceiling(loom_s16 left, loom_s16 right, loom_s16 head, loom_s16 reach);

void loom_pvs_actor_body(LoomActorBodyStep *step, const LoomPlatformerBody *body);

#endif
