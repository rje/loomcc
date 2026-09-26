/* The C that body.asm's loom_pvs_animation_pass replaced (Loom 5184be9^:
 * runtime/src/animation.c:387-420, loom_animation_update's per-player loop
 * up to the due test), cut out as a function with the assembly routine's
 * name and interface. Where the C went on to advance a due player there and
 * then (the next frame, the loop, the pose), the assembly lists the player
 * on `due` for C to advance afterwards; this does the same. The C reads
 * loom_animation_state directly, so its loom_pvs_animation_bind (which
 * hands the assembly the state's arrays) has nothing to do. */
#include "loom_types.h"

void loom_pvs_animation_bind(loom_u8 *playing, loom_u16 *elapsed_ticks,
                             loom_u8 *frame_index,
                             const LoomAnimationFrame **frames)
{
    (void)playing;
    (void)elapsed_ticks;
    (void)frame_index;
    (void)frames;
}

loom_u16 loom_pvs_animation_pass(loom_u16 count, loom_u8 *due)
{
    const LoomAnimationFrame **frames;
    loom_u8 index;
    loom_u16 due_count;

    due_count = 0u;
    /* A pointer walk over the players: an indexed struct access is a
     * multiply helper call on 816-tcc. */
    frames = loom_animation_state.frames;
    for (index = 0u;
         index < count;
         ++index, ++frames) {
        const LoomAnimationFrame *frame;

        if (loom_animation_state.playing[index] == LOOM_FALSE) {
            continue;
        }
        frame = &(*frames)[loom_animation_state.frame_index[index]];
        ++loom_animation_state.elapsed_ticks[index];
        if (loom_animation_state.elapsed_ticks[index] <
            frame->duration_ticks) {
            continue;
        }
        due[due_count] = index;
        ++due_count;
    }
    return due_count;
}
