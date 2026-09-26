#ifndef LOOM_GENERATED_VARIABLES_H
#define LOOM_GENERATED_VARIABLES_H

/* Typed game-state variables from Data/variables.loom-vars.json. */
#include <loom/variables.h>

#define LOOM_GENERATED_VARIABLE_COUNT ((loom_u16)5u)
#define LOOM_GENERATED_VARIABLE_PERSISTENT_WORDS ((loom_u16)1u)
#define LOOM_GENERATED_VARIABLE_SCENE_WORDS ((loom_u16)0u)
#define LOOM_GENERATED_VARIABLE_FRAME_WORDS ((loom_u16)0u)

/* adventure_flag_0: persistent bool backed by adventure flag 0 */
#define LOOM_VAR_ADVENTURE_FLAG_0 ((LoomVariableHandle)0u)
#define LOOM_VAR_ADVENTURE_FLAG_0_DEFAULT ((loom_u16)0u)
/* adventure_flag_1: persistent bool backed by adventure flag 1 */
#define LOOM_VAR_ADVENTURE_FLAG_1 ((LoomVariableHandle)1u)
#define LOOM_VAR_ADVENTURE_FLAG_1_DEFAULT ((loom_u16)0u)
/* adventure_flag_2: persistent bool backed by adventure flag 2 */
#define LOOM_VAR_ADVENTURE_FLAG_2 ((LoomVariableHandle)2u)
#define LOOM_VAR_ADVENTURE_FLAG_2_DEFAULT ((loom_u16)0u)
/* adventure_flag_3: persistent bool backed by adventure flag 3 */
#define LOOM_VAR_ADVENTURE_FLAG_3 ((LoomVariableHandle)3u)
#define LOOM_VAR_ADVENTURE_FLAG_3_DEFAULT ((loom_u16)0u)
/* health: persistent u16 */
#define LOOM_VAR_HEALTH ((LoomVariableHandle)4u)
#define LOOM_VAR_HEALTH_DEFAULT ((loom_u16)6u)
#define LOOM_VAR_HEALTH_WORD (loom_generated_variable_persistent[0u])

#endif
