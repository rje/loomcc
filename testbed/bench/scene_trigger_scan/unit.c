/* The C that runtime/backends/pvsneslib/src/scene.asm replaced
 * (Loom 7b4d960^:runtime/src/scene.c:234-246, the box test inside
 * loom_scene_check_triggers' trigger loop), cut out as a function with the
 * assembly routine's name and interface: every trigger's box test at once,
 * returning bit i set when trigger i overlaps the player box. The test is
 * the parent's condition verbatim; 7b4d960 kept the same test as the host
 * rendition loom_scene_intersect_mask (scene.c:145-164 at 7b4d960). */
#include "loom_scene.h"

loom_u16 loom_pvs_scene_intersect_mask(const LoomSceneBox *player,
                                       const LoomSceneTrigger *trigger,
                                       loom_u8 count)
{
    loom_u16 mask;
    loom_u16 index;
    loom_u16 bit;

    mask = 0u;
    bit = 1u;
    for (index = 0u; index < count;
         ++index, ++trigger, bit = (loom_u16)(bit << 1)) {
        /* The box test first: most triggers are not under the player, and
         * the gate check walks the adventure kit's flag word. */
        if (player->left >= (loom_s16)(trigger->x + (loom_s16)trigger->width) ||
            player->right <= trigger->x ||
            player->top >= (loom_s16)(trigger->y + (loom_s16)trigger->height) ||
            player->bottom <= trigger->y) {
            continue;
        }
        mask |= bit;
    }
    return mask;
}
