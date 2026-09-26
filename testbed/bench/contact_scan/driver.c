/* contact_scan: combat's contact query over a pool of eight actors (Loom
 * 925fc3a). Four types: a walker, a large blob with an offset hit box, a
 * harmless prop (no contact damage) and the player's own shot. Some slots are
 * dead or asleep. Four player boxes, each scanned the way loom_combat_update
 * walks it (from the last hit + 1 until 0xffff), so every hit is found. */
#include "bench.h"
#include "loom_actor.h"

LoomActorPool loom_actor_pool;

/* body.asm's pool bindings (Loom binds them once per activation through
 * loom_pvs_actor_bind / loom_pvs_actor_bind_pass); only the asm reads them. */
loom_u8 *loom_pvs_actor_arr_alive;
loom_u8 *loom_pvs_actor_arr_awake;
const LoomActorType **loom_pvs_actor_arr_type_ptr;
loom_s16 *loom_pvs_actor_arr_x;
loom_s16 *loom_pvs_actor_arr_y;

static LoomActorType types[4];

#define N 8
#define BOXES 4
#define MAXHITS 4

static loom_u16 hits[BOXES][MAXHITS + 1];

static void place(loom_u8 i, loom_u8 type, loom_s16 x, loom_s16 y, loom_u8 alive, loom_u8 awake)
{
    loom_actor_pool.type_index[i] = type;
    loom_actor_pool.type_ptr[i] = &types[type];
    loom_actor_pool.x[i] = x;
    loom_actor_pool.y[i] = y;
    loom_actor_pool.alive[i] = alive;
    loom_actor_pool.awake[i] = awake;
}

static void set_hit(LoomActorType *t, loom_s16 hx, loom_s16 hy, loom_u16 w, loom_u16 h, loom_u8 damage)
{
    t->hit_x = hx;
    t->hit_y = hy;
    t->hit_width = w;
    t->hit_height = h;
    t->contact_damage = damage;
    t->health = 1u;
}

void bench_setup(void)
{
    set_hit(&types[0], -8, -16, 16, 16, 1u);   /* walker */
    set_hit(&types[1], -20, -40, 40, 36, 2u);  /* blob */
    set_hit(&types[2], -8, -8, 16, 16, 0u);    /* prop: no damage */
    set_hit(&types[3], -4, -4, 8, 8, 1u);      /* the player's shot */

    place(0, 0, 100, 200, 1u, 1u);
    place(1, 2, 120, 200, 1u, 1u);
    place(2, 1, 180, 190, 1u, 1u);
    place(3, 0, 110, 196, 1u, 0u);   /* asleep */
    place(4, 3, 104, 190, 1u, 1u);   /* a shot */
    place(5, 0, 106, 204, 0u, 1u);   /* dead */
    place(6, 0, 170, 180, 1u, 1u);
    place(7, 1, -30, 100, 1u, 1u);
    loom_actor_pool.count = N;

    loom_pvs_actor_arr_alive = loom_actor_pool.alive;
    loom_pvs_actor_arr_awake = loom_actor_pool.awake;
    loom_pvs_actor_arr_type_ptr = loom_actor_pool.type_ptr;
    loom_pvs_actor_arr_x = loom_actor_pool.x;
    loom_pvs_actor_arr_y = loom_actor_pool.y;
}

/* Every hit for one player box, as loom_combat_update's loop walks them. */
#define SCAN(b, l, t, r, bot)                                                   \
    do {                                                                        \
        loom_u16 start = 0u, found, n = 0u;                                     \
        for (;;) {                                                              \
            found = loom_pvs_combat_touch(start, N, (l), (t), (r), (bot), &types[3]); \
            hits[b][n + 1u] = found;                                            \
            if (found == 0xffffu || n == MAXHITS - 1u) break;                  \
            ++n;                                                                \
            start = (loom_u16)(found + 1u);                                     \
        }                                                                       \
        hits[b][0] = n;                                                         \
    } while (0)

void bench_run(void)
{
    SCAN(0, 96, 180, 111, 203);   /* over the walker, the prop, the shot, the dead and sleeping ones */
    SCAN(1, 150, 150, 165, 175);  /* inside the blob at 2 and the walker at 6 */
    SCAN(2, -40, 60, -25, 90);    /* the blob off the left edge */
    SCAN(3, 0, 0, 15, 15);        /* nothing */
}

void bench_check(void)
{
    loom_u16 b, i;
    for (b = 0u; b < BOXES; b++) {
        loom_u16 fold = hits[b][0];
        for (i = 1u; i <= MAXHITS; i++)
            fold = (loom_u16)(fold * 31u + hits[b][i]);
        BENCH_OUT(hits[b][0]);
        BENCH_OUT(hits[b][1]);
        BENCH_OUT(fold);
    }
}
