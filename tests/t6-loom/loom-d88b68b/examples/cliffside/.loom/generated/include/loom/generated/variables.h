#ifndef LOOM_GENERATED_VARIABLES_H
#define LOOM_GENERATED_VARIABLES_H

/* Typed game-state variables from Data/variables.loom-vars.json. */
#include <loom/variables.h>

#define LOOM_GENERATED_VARIABLE_COUNT ((loom_u16)1u)
#define LOOM_GENERATED_VARIABLE_PERSISTENT_WORDS ((loom_u16)1u)
#define LOOM_GENERATED_VARIABLE_SCENE_WORDS ((loom_u16)0u)
#define LOOM_GENERATED_VARIABLE_FRAME_WORDS ((loom_u16)0u)

/* health: persistent u16 */
#define LOOM_VAR_HEALTH ((LoomVariableHandle)0u)
#define LOOM_VAR_HEALTH_DEFAULT ((loom_u16)8u)
#define LOOM_VAR_HEALTH_WORD (loom_generated_variable_persistent[0u])

#endif
