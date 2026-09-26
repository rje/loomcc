/* The C that body.asm's loom_actor_controller_position replaced (Loom
 * a937adb): a937adb^:runtime/src/actor.c:239-253, verbatim. The pool and
 * loom_actor_controller_slot are driver state. */
#include "loom_actor.h"

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
