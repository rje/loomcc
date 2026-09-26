/* player2_position: the second player's position query over a bound actor
 * pool, 8 calls, each after setting the controller slot the way activation
 * and a join would leave it: no controller, a dead slot, a live slot whose
 * behaviour has not started (not joined), and joined slots at the pool's
 * start, middle and end with negative and large coordinates. The outputs
 * start as a sentinel so a query that writes nothing is visible.
 *
 * body.asm reads the pool through the array pointers
 * loom_actor_pass's bind (loom_pvs_actor_bind_pass) stores; the driver
 * stands in for the bind and points them at the pool's arrays. The C
 * variant ignores them. */
#include "bench.h"
#include "loom_actor.h"

LoomActorPool loom_actor_pool;
loom_u8 loom_actor_controller_slot = LOOM_ACTOR_INVALID_INDEX;

/* body.asm's bound pool arrays (asm RAM there, driver state here). */
void *loom_pvs_actor_arr_alive;
void *loom_pvs_actor_arr_behavior_a;
void *loom_pvs_actor_arr_x;
void *loom_pvs_actor_arr_y;

#define QUERIES 8
#define SENTINEL ((loom_s16)0x5a5a)

static loom_s16 xs[QUERIES];
static loom_s16 ys[QUERIES];
static loom_u8 found[QUERIES];

void bench_setup(void)
{
    unsigned short i;

    for (i = 0; i < LOOM_ACTOR_CAPACITY; i++) {
        loom_actor_pool.alive[i] = (loom_u8)((i % 3u) != 1u);
        loom_actor_pool.behavior_a[i] = (loom_u8)(i & 1u);
        loom_actor_pool.x[i] = (loom_s16)(i * 37u - 200u);
        loom_actor_pool.y[i] = (loom_s16)(1000u - i * 53u);
    }
    loom_actor_pool.alive[0] = LOOM_TRUE;
    loom_actor_pool.behavior_a[0] = 2u;
    loom_actor_pool.x[0] = -40;
    loom_actor_pool.y[0] = 200;
    loom_actor_pool.alive[31] = LOOM_TRUE;
    loom_actor_pool.count = LOOM_ACTOR_CAPACITY;
    loom_actor_pool.initialized = LOOM_TRUE;
    for (i = 0; i < QUERIES; i++) {
        xs[i] = SENTINEL;
        ys[i] = SENTINEL;
    }
    loom_pvs_actor_arr_alive = loom_actor_pool.alive;
    loom_pvs_actor_arr_behavior_a = loom_actor_pool.behavior_a;
    loom_pvs_actor_arr_x = loom_actor_pool.x;
    loom_pvs_actor_arr_y = loom_actor_pool.y;
}

#define Q(i, slot)                                                  \
    loom_actor_controller_slot = (slot);                            \
    found[i] = loom_actor_controller_position(&xs[i], &ys[i])

void bench_run(void)
{
    Q(0, 0xffu);  /* no controller actor */
    Q(1, 0u);     /* joined, x -40, y 200 */
    Q(2, 4u);     /* dead (4 % 3 == 1) */
    Q(3, 6u);     /* live, behaviour_a 0: not joined */
    Q(4, 9u);     /* joined */
    Q(5, 16u);    /* dead */
    Q(6, 29u);    /* joined */
    Q(7, 31u);    /* joined, the last slot */
}

void bench_check(void)
{
    unsigned short i, word = 0;

    for (i = 0; i < QUERIES; i++)
        word |= (unsigned short)((found[i] & 0xffu) << (2u * i));
    BENCH_OUT(word);
    for (i = 0; i < QUERIES; i++) {
        BENCH_OUT((loom_u16)xs[i]);
        BENCH_OUT((loom_u16)ys[i]);
    }
}
