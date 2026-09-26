/* solid_actor_query: the pool's solid-actor queries (Loom ea4e3af) the way
 * the assembly bodies ask them: a floor under a body (the highest visible
 * solid actor top in a box) and a solid actor in its way. Eight actors, five
 * of them solid: two platforms at different heights over the same columns,
 * a crate, a hidden (gated) block and a solid slot the scene never mapped to
 * a sprite. Five floor queries and five blocking queries. */
#include "bench.h"
#include "loom_actor.h"

LoomActorPool loom_actor_pool;
loom_u8 loom_movement_solid_actors;
loom_s16 loom_movement_probe_floor;
loom_u8 loom_mode1_sprite_visible[LOOM_FRAME_OAM_CAPACITY];

/* body.asm's bindings (loom_pvs_actor_bind_pass and loom_pvs_solid_bind at
 * activation in Loom); only the asm reads them. */
loom_u8 *loom_pvs_actor_arr_sprite_index;
loom_s16 *loom_pvs_actor_arr_x;
loom_s16 *loom_pvs_actor_arr_y;
const LoomActorType **loom_pvs_actor_arr_type_ptr;
const loom_u8 *loom_pvs_solid_slots;
const loom_u8 *loom_pvs_solid_visible;

static LoomActorType types[3];

#define QUERIES 5
static loom_u16 floor_found[QUERIES];
static loom_s16 floor_row[QUERIES];
static loom_u16 blocked[QUERIES];

static void place(loom_u8 i, loom_u8 type, loom_s16 x, loom_s16 y, loom_u8 sprite)
{
    loom_actor_pool.alive[i] = LOOM_TRUE;
    loom_actor_pool.type_index[i] = type;
    loom_actor_pool.type_ptr[i] = &types[type];
    loom_actor_pool.x[i] = x;
    loom_actor_pool.y[i] = y;
    loom_actor_pool.sprite_index[i] = sprite;
}

static void box(LoomActorType *t, loom_s16 bx, loom_s16 by, loom_u16 w, loom_u16 h, loom_u8 collision)
{
    t->box_x = bx;
    t->box_y = by;
    t->box_width = w;
    t->box_height = h;
    t->collision = collision;
}

void bench_setup(void)
{
    loom_u8 i;

    box(&types[0], -24, -8, 48, 8, 2u);     /* a platform, anchored at its top centre */
    box(&types[1], -8, -16, 16, 16, 2u);    /* a crate, anchored at its feet */
    box(&types[2], -8, -16, 16, 16, 0u);    /* a walker (not solid) */

    place(0, 2, 40, 100, 0u);
    place(1, 0, 120, 140, 1u);              /* platform, top 132 */
    place(2, 2, 60, 150, 2u);
    place(3, 0, 110, 116, 3u);              /* a higher platform over the same columns, top 108 */
    place(4, 1, 200, 176, 4u);              /* crate, 192..207 x 160..175 */
    place(5, 1, 250, 176, 5u);              /* hidden block */
    place(6, 1, 280, 176, 0xffu);           /* never mapped */
    place(7, 2, 10, 10, 6u);
    loom_actor_pool.count = 8u;
    loom_actor_pool.initialized = LOOM_TRUE;
    loom_actor_pool.solid_slots[0] = 1u;
    loom_actor_pool.solid_slots[1] = 4u;
    loom_actor_pool.solid_slots[2] = 3u;
    loom_actor_pool.solid_slots[3] = 5u;
    loom_actor_pool.solid_slots[4] = 6u;
    loom_actor_pool.solid_count = 5u;
    loom_movement_solid_actors = loom_actor_pool.solid_count;
    for (i = 0u; i < 7u; i++)
        loom_mode1_sprite_visible[i] = LOOM_TRUE;
    loom_mode1_sprite_visible[5] = LOOM_FALSE;
    loom_movement_probe_floor = -1;

    loom_pvs_actor_arr_sprite_index = loom_actor_pool.sprite_index;
    loom_pvs_actor_arr_x = loom_actor_pool.x;
    loom_pvs_actor_arr_y = loom_actor_pool.y;
    loom_pvs_actor_arr_type_ptr = loom_actor_pool.type_ptr;
    loom_pvs_solid_slots = loom_actor_pool.solid_slots;
    loom_pvs_solid_visible = loom_mode1_sprite_visible;
}

/* A floor query: left, right, the row under the feet, the reach. */
#define FLOOR(i, l, r, t, b)                                                    \
    (floor_found[i] = loom_movement_actor_floor_probe((l), (r), (t), (b)),      \
     floor_row[i] = loom_movement_probe_floor)
#define BLOCK(i, l, t, r, b) (blocked[i] = loom_movement_actor_block_probe((l), (t), (r), (b)))

void bench_run(void)
{
    FLOOR(0, 100, 115, 100, 140);   /* both platforms in reach: the higher one */
    FLOOR(1, 100, 115, 112, 140);   /* only the lower one */
    FLOOR(2, 196, 211, 150, 170);   /* the crate */
    FLOOR(3, 244, 300, 150, 190);   /* the hidden block and the unmapped slot: none */
    FLOOR(4, 20, 40, 0, 200);       /* nothing under these columns */
    BLOCK(0, 180, 160, 195, 175);   /* overlaps the crate's left edge */
    BLOCK(1, 208, 160, 223, 175);   /* just right of the crate */
    BLOCK(2, 245, 160, 260, 175);   /* the hidden block: clear */
    BLOCK(3, 90, 100, 105, 110);    /* the higher platform */
    BLOCK(4, -20, -20, 5, 5);       /* off the corner */
}

void bench_check(void)
{
    loom_u16 i;

    for (i = 0u; i < QUERIES; i++) {
        BENCH_OUT(floor_found[i]);
        BENCH_OUT(floor_row[i]);
    }
    for (i = 0u; i < QUERIES; i++)
        BENCH_OUT(blocked[i]);
}
