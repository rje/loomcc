/* Generated direct dispatch: no console-side function-pointer scheduler. */
#include <loom/generated/user_hooks.h>

LoomStatus loom_generated_dispatch_user_hook(loom_u16 hook_id)
{
switch (hook_id) {
    case 21993u:
        update_player();
        return LOOM_STATUS_OK;
    case 39544u:
        on_summit();
        return LOOM_STATUS_OK;
    default:
return LOOM_STATUS_INVALID_HANDLE;
}
}
