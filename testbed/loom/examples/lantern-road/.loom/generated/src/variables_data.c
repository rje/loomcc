/* Generated variable storage: fixed 16-bit word blocks per lifetime. */
#include <loom/generated/variables.h>
#include <loom/adventure.h>

const loom_u16 loom_generated_variable_count = 5u;
const LoomVariableRecord loom_generated_variables[5] = {
    { 0u, 2u, LOOM_VARIABLE_KIND_ADVENTURE_BOOL, LOOM_VARIABLE_LIFETIME_PERSISTENT, 1u, 0u }, /* adventure_flag_0 */
    { 1u, 2u, LOOM_VARIABLE_KIND_ADVENTURE_BOOL, LOOM_VARIABLE_LIFETIME_PERSISTENT, 1u, 0u }, /* adventure_flag_1 */
    { 2u, 2u, LOOM_VARIABLE_KIND_ADVENTURE_BOOL, LOOM_VARIABLE_LIFETIME_PERSISTENT, 1u, 0u }, /* adventure_flag_2 */
    { 3u, 2u, LOOM_VARIABLE_KIND_ADVENTURE_BOOL, LOOM_VARIABLE_LIFETIME_PERSISTENT, 1u, 0u }, /* adventure_flag_3 */
    { 0u, 0u, LOOM_VARIABLE_KIND_U16, LOOM_VARIABLE_LIFETIME_PERSISTENT, 1u, 0u }, /* health */
};

const loom_u16 loom_generated_variable_persistent_count = 1u;
loom_u16 loom_generated_variable_persistent[1];
const loom_u16 loom_generated_variable_persistent_defaults[1] = { 6u };

const loom_u16 loom_generated_variable_scene_count = 0u;
loom_u16 loom_generated_variable_scene[1];
const loom_u16 loom_generated_variable_scene_defaults[1] = { 0u };

const loom_u16 loom_generated_variable_frame_count = 0u;
loom_u16 loom_generated_variable_frame[1];
const loom_u16 loom_generated_variable_frame_defaults[1] = { 0u };

loom_u16 loom_generated_variable_external_get(loom_u16 storage_index)
{
    return loom_adventure_flag_is_set((loom_u8)storage_index) != LOOM_FALSE ? 1u : 0u;
}

LoomStatus loom_generated_variable_external_set(loom_u16 storage_index, loom_u16 value)
{
    if (value != 0u) {
        return loom_adventure_flag_set((loom_u8)storage_index);
    }
    return loom_adventure_flag_clear((loom_u8)storage_index);
}
