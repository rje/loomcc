/* player_tick: the player's platformer tick (Loom 925fc3a) in a 20x14-tile
 * room with a slope and a ceiling block, on a run-and-dash body (PHY-002).
 * Three ticks, each from a state the driver sets up between the calls (as
 * earlier ticks would have left it): a skid against a dash that still
 * carries the body onto a slope, a running jump (the jump scaled by the
 * run), and a released jump cut that meets the ceiling. */
#include "bench.h"
#include "loom_movement.h"

LoomMovementState loom_movement_state;
LoomMovementGrid loom_movement_grid;

/* The generated actor hooks for a project without placed actors
 * (crates/loom-app/src/generate/output.rs at 925fc3a^). */
loom_u8 loom_generated_actor_blocks_box(loom_s16 left, loom_s16 top,
                                        loom_s16 right, loom_s16 bottom)
{
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    return LOOM_FALSE;
}

loom_u8 loom_generated_actor_floor_below(loom_s16 left, loom_s16 right,
                                         loom_s16 top, loom_s16 bottom,
                                         loom_s16 *floor)
{
    (void)left;
    (void)right;
    (void)top;
    (void)bottom;
    (void)floor;
    return 0xffu;
}

#define W 20
#define H 14
#define TICKS 3

static loom_u8 cells[W * H];
static LoomPlatformerBody body;
static LoomMovementScene scene;
static LoomMovementState after[TICKS];

/* bench_setup multiplies and divides by these so 816-tcc links tcc__mul and
 * tcc__udiv (the C unit's helpers) into the asm variant too: the harness
 * looks every helper label up in both. Setup is outside the measurement. */
static loom_u16 setup_four = 4u;
static loom_u16 setup_five = 5u;

static void put(loom_s16 x, loom_s16 y, loom_u8 sub_x, loom_u8 sub_y, loom_s16 vx,
                loom_s16 vy, loom_u8 on_ground, loom_u8 jumping, loom_u8 dash)
{
    loom_movement_state.player_x = x;
    loom_movement_state.player_y = y;
    loom_movement_state.subpixel_x = sub_x;
    loom_movement_state.subpixel_y = sub_y;
    loom_movement_state.velocity_x = vx;
    loom_movement_state.velocity_y = vy;
    loom_movement_state.on_ground = on_ground;
    loom_movement_state.jumping = jumping;
    loom_movement_state.dash_meter = dash;
}

void bench_setup(void)
{
    loom_u16 x, y;

    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            cells[y * W + x] = (x == 0 || x == W - 1 || y >= 12) ? 1u : 0u;
    cells[11 * W + 10] = 3u;               /* a slope up to the right, x 160..175 */
    cells[11 * W + 11] = 1u;               /* its plateau */
    cells[11 * W + 12] = 1u;
    cells[6 * W + 4] = 1u;                 /* a ceiling block, y 96..111 */
    cells[6 * W + 5] = 1u;
    loom_movement_grid.cells = cells;
    loom_movement_grid.pixel_width = W * 16;
    loom_movement_grid.pixel_height = H * 16;
    for (y = 0; y < H; y++)
        loom_movement_grid.row_offsets[y] = (loom_u16)(y * W);

    body.max_speed = 0x0180u;
    body.acceleration = 0x0018u;
    body.friction = 0x0010u;
    body.air_control = 0x000cu;
    body.gravity = 0x0040u;
    body.terminal_velocity = 0x0500u;
    body.jump_speed = 0x0480u;
    body.jump_cut = 0x0200u;
    body.coyote_ticks = 4u;
    body.buffer_ticks = 4u;
    body.flags = LOOM_PLATFORMER_FLAG_RUN;
    body.run_speed = 0x0240u;
    body.dash_speed = 0x0300u;
    body.skid = 0x0030u;
    body.jump_speed_fast = 0x0580u;
    body.gravity_hold = 0x0020u;
    body.dash_ticks = (loom_u8)((loom_u16)(setup_four * setup_five) / (loom_u16)(setup_five / setup_four));

    scene.pixel_width = W * 16;
    scene.pixel_height = H * 16;
    scene.collider_x = -8;
    scene.collider_y = -24;
    scene.collider_width = 16u;
    scene.collider_height = 24u;
    scene.collision_width = W;
    scene.collision_height = H;
    scene.body = 2u;
    scene.collision_cells = cells;
    scene.platformer = &body;

    loom_movement_state.scene = &scene;
    loom_movement_state.initialized = LOOM_TRUE;
    loom_movement_state.riding = 0xffu;
}

#define TICK(i, held, pressed)                  \
    do {                                        \
        loom_pvs_player_tick((held), (pressed)); \
        after[i] = loom_movement_state;         \
    } while (0)

void bench_run(void)
{
    /* x, y, sub_x, sub_y, vx, vy, on_ground, jumping, dash_meter */
    put(150, 191, 0x40u, 0u, 0x02f0, 0, 1u, 0u, 19u);
    TICK(0, LOOM_BUTTON_LEFT | LOOM_BUTTON_Y, 0u);
    put(40, 191, 0u, 0u, 0x0230, 0, 1u, 0u, 3u);
    TICK(1, LOOM_BUTTON_RIGHT | LOOM_BUTTON_Y | LOOM_BUTTON_B, LOOM_BUTTON_B);
    put(72, 137, 0x80u, 0x20u, 0x0100, -0x0450, 0u, 1u, 0u);
    TICK(2, LOOM_BUTTON_LEFT, 0u);

}

void bench_check(void)
{
    loom_u16 i, k;

    for (i = 0; i < TICKS; i++) {
        const unsigned char *bytes = (const unsigned char *)&after[i];
        loom_u16 fold = 0u;

        /* Every field before the scene pointer (36 bytes). */
        for (k = 0; k < 36u; k++)
            fold = (loom_u16)((loom_u16)(fold << 5) + (loom_u16)(fold >> 11) + bytes[k]);
        BENCH_OUT(after[i].player_x);
        BENCH_OUT(after[i].player_y);
        BENCH_OUT(after[i].velocity_x);
        BENCH_OUT(after[i].velocity_y);
        BENCH_OUT((loom_u16)(after[i].on_ground | after[i].wall << 2 | after[i].landed << 4 |
                             after[i].jumping << 5 | after[i].coyote_left << 8 | after[i].dash_meter << 11));
        BENCH_OUT(fold);
    }
}
