/* Generated variable storage: fixed 16-bit word blocks per lifetime. */
#include <loom/generated/variables.h>

const loom_u16 loom_generated_variable_count = 1u;
const LoomVariableRecord loom_generated_variables[1] = {
    { 0u, 0u, LOOM_VARIABLE_KIND_U16, LOOM_VARIABLE_LIFETIME_PERSISTENT, 1u, 0u }, /* health */
};

const loom_u16 loom_generated_variable_persistent_count = 1u;
loom_u16 loom_generated_variable_persistent[1];
const loom_u16 loom_generated_variable_persistent_defaults[1] = { 8u };

const loom_u16 loom_generated_variable_scene_count = 0u;
loom_u16 loom_generated_variable_scene[1];
const loom_u16 loom_generated_variable_scene_defaults[1] = { 0u };

const loom_u16 loom_generated_variable_frame_count = 0u;
loom_u16 loom_generated_variable_frame[1];
const loom_u16 loom_generated_variable_frame_defaults[1] = { 0u };

loom_u16 loom_generated_variable_external_get(loom_u16 storage_index)
{
    (void)storage_index;
    return 0u;
}

LoomStatus loom_generated_variable_external_set(loom_u16 storage_index, loom_u16 value)
{
    (void)storage_index;
    (void)value;
    return LOOM_STATUS_INVALID_HANDLE;
}
