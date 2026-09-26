#ifndef LOOM_GENERATED_USER_HOOKS_H
#define LOOM_GENERATED_USER_HOOKS_H

/* Implement enabled hook symbols in authored Code/Portable source. */
#include <loom/types.h>

#define LOOM_GENERATED_USER_HOOK_COUNT ((loom_u16)1u)
#define LOOM_GENERATED_USER_HOOK_ID_000000000000736018CF5B7BD3D78710 ((loom_u16)42035u)

/* Called once per logical tick, after the input snapshot and before
 * movement, because [runtime] game_update is set. Implement it in authored
 * Code/Portable source. */
void loom_game_update(void);

void on_crossing_welcome(void);

LoomStatus loom_generated_dispatch_user_hook(loom_u16 hook_id);

#endif
