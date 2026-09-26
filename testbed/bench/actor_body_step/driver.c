/* actor_body_step: the actor platformer body step (Loom 13d8987) on its
 * record, six bodies in a 20x14-tile room: a walker turning at a ledge, a
 * walker running into a wall, a jump that hits a ceiling, a released jump
 * being cut, a grounded body braking towards a slope, and a body at the top
 * of its arc (no vertical scan). The driver fills each record the way actor.c's
 * console step does before the call; the tile probes the step calls
 * (loom_pvs_body_probe_x/_scan_floor/_scan_ceiling, body.asm at 13d8987^)
 * are provided here in C, built from the portable loops they stand in for
 * (actor.c:628-649, 674-701, 753-768 at 13d8987^), and are not measured as
 * the unit. */
#include "bench.h"
#include "loom_body.h"

LoomMovementGrid loom_movement_grid;

#define W 20
#define H 14
#define CALLS 6

static loom_u8 cells[W * H];
static LoomPlatformerBody body;
static LoomActorBodyStep steps[CALLS];

static loom_s16 floor_in_cell(loom_u8 cell, loom_s16 x, loom_s16 top)
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

static loom_u8 column_solid(loom_s16 x, loom_s16 top, loom_s16 bottom)
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

loom_s16 loom_pvs_body_probe_x(loom_s16 edge, loom_s16 delta, loom_s16 top, loom_s16 wall_bottom)
{
    loom_s16 probe;

    for (probe = (loom_s16)(edge + (delta > 0 ? 1 : -1));
         delta > 0 ? probe <= (loom_s16)(edge + delta)
                   : probe >= (loom_s16)(edge + delta);
         probe = (loom_s16)(probe + (delta > 0 ? 1 : -1))) {
        if (column_solid(probe, top, wall_bottom) != LOOM_FALSE) {
            return probe;
        }
        if (delta > 0 ? ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) !=
                         LOOM_MOVEMENT_TILE_PIXELS - 1)
                      : ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) != 0)) {
            probe = delta > 0
                        ? (loom_s16)(probe | (LOOM_MOVEMENT_TILE_PIXELS - 1))
                        : (loom_s16)(probe & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
        }
    }
    return (loom_s16)0x7fff;
}

loom_s16 loom_pvs_body_scan_floor(loom_s16 left, loom_s16 right, loom_s16 bottom, loom_s16 reach, loom_s16 sensor_x)
{
    loom_s16 row_top;
    loom_s16 landed;
    loom_u8 cell;

    landed = -1;
    for (row_top = (loom_s16)((bottom + 1) & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
         row_top <= reach && landed < 0;
         row_top = (loom_s16)(row_top + LOOM_MOVEMENT_TILE_PIXELS)) {
        loom_s16 x;

        LOOM_MOVEMENT_CELL_AT(cell, sensor_x, row_top);
        if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ||
            cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
            loom_s16 floor;

            floor = floor_in_cell(cell, sensor_x, row_top);
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
    return landed;
}

loom_s16 loom_pvs_body_scan_ceiling(loom_s16 left, loom_s16 right, loom_s16 top, loom_s16 reach)
{
    loom_s16 row_top;
    loom_s16 stopped;
    loom_u8 cell;

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
    return stopped;
}

/* What actor.c's console step copies into the record before the call. The
 * anchor is the feet's centre: a 16x16 box at (-8, -16). */
static void fill(loom_u8 i, loom_s16 x, loom_s16 y, loom_u8 sub_x, loom_u8 sub_y,
                 loom_s16 vx, loom_s16 vy, loom_s8 intent_x, loom_s8 intent_y,
                 loom_u8 grounded)
{
    LoomActorBodyStep *s = &steps[i];

    s->x = x;
    s->y = y;
    s->vx = vx;
    s->vy = vy;
    s->box_x = -8;
    s->box_y = -16;
    s->box_w = 16u;
    s->box_h = 16u;
    s->sub_x = sub_x;
    s->sub_y = sub_y;
    s->intent_x = intent_x;
    s->intent_y = intent_y;
    s->grounded = grounded;
    s->turn_at_ledges =
        (loom_u8)((body.flags & LOOM_PLATFORMER_FLAG_TURN_AT_LEDGES) != 0u);
}

void bench_setup(void)
{
    loom_u16 x, y;

    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            cells[y * W + x] = (x == 0 || x == W - 1 || y == 12 || y == 13) ? 1u : 0u;
    for (x = 5; x <= 8; x++)
        cells[8 * W + x] = 1u;             /* a ledge platform, y 128..143 */
    for (x = 12; x <= 14; x++)
        cells[9 * W + x] = 2u;             /* a one-way platform, y 144..159 */
    cells[11 * W + 16] = 3u;               /* a slope up to the right, y 176..191 */
    cells[11 * W + 17] = 1u;
    loom_movement_grid.cells = cells;
    loom_movement_grid.pixel_width = W * 16;
    loom_movement_grid.pixel_height = H * 16;
    for (y = 0; y < H; y++)
        loom_movement_grid.row_offsets[y] = (loom_u16)(y * W);

    body.max_speed = 0x0180u;
    body.acceleration = 0x0020u;
    body.friction = 0x0018u;
    body.air_control = 0x0010u;
    body.gravity = 0x0040u;
    body.terminal_velocity = 0x0400u;
    body.jump_speed = 0x0500u;
    body.jump_cut = 0x0200u;
    body.flags = LOOM_PLATFORMER_FLAG_TURN_AT_LEDGES;

    /* x, y, sub_x, sub_y, vx, vy, intent_x, intent_y, grounded */
    fill(0, 137, 127, 0x80u, 0u, 0x0100, 0, 1, 0, 1u);           /* ledge ahead: turn */
    fill(1, 26, 191, 0x10u, 0u, -0x0170, 0, -1, 0, 1u);          /* into the left wall */
    fill(2, 104, 162, 0u, 0u, 0x0080, 0, 1, -1, 1u);             /* jumps under the ledge platform */
    fill(3, 70, 100, 0x20u, 0x90u, -0x0090, -0x0400, -1, 0, 0u); /* a released jump: cut */
    fill(4, 250, 191, 0xf0u, 0u, 0x0110, 0, 0, 0, 1u);           /* braking towards the slope */
    fill(5, 150, 60, 0u, 0x10u, -0x0020, -0x0040, 0, 1, 0u);     /* the top of the arc */
}

void bench_run(void)
{
    loom_pvs_actor_body(&steps[0], &body);
    loom_pvs_actor_body(&steps[1], &body);
    loom_pvs_actor_body(&steps[2], &body);
    loom_pvs_actor_body(&steps[3], &body);
    loom_pvs_actor_body(&steps[4], &body);
    loom_pvs_actor_body(&steps[5], &body);
}

void bench_check(void)
{
    loom_u16 i, k;

    for (i = 0; i < CALLS; i++) {
        const unsigned char *bytes = (const unsigned char *)&steps[i];
        loom_u16 fold = 0u;

        for (k = 0; k < sizeof(LoomActorBodyStep); k++)
            fold = (loom_u16)((loom_u16)(fold << 5) + (loom_u16)(fold >> 11) + bytes[k]);
        BENCH_OUT(steps[i].x);
        BENCH_OUT(steps[i].next_y);
        BENCH_OUT((loom_u16)((loom_u16)steps[i].phase << 8 | steps[i].flags));
        BENCH_OUT(fold);
    }
}
