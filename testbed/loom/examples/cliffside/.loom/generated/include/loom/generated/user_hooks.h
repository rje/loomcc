#ifndef LOOM_GENERATED_USER_HOOKS_H
#define LOOM_GENERATED_USER_HOOKS_H

/* Implement enabled hook symbols in authored Code/Portable source. */
#include <loom/types.h>

#define LOOM_GENERATED_USER_HOOK_COUNT ((loom_u16)2u)
#define LOOM_GENERATED_USER_HOOK_ID_000000000000D46918C4ECA8B6B13340 ((loom_u16)21993u)
#define LOOM_GENERATED_USER_HOOK_ID_000000000000D46918C46CA8B2160428 ((loom_u16)39544u)

void on_summit(void);
void update_player(void);

LoomStatus loom_generated_dispatch_user_hook(loom_u16 hook_id);

#endif
