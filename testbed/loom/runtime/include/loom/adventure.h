#ifndef LOOM_ADVENTURE_H
#define LOOM_ADVENTURE_H

#include <loom/input.h>

#define LOOM_ADVENTURE_FLAG_CAPACITY ((loom_u8)16u)
#define LOOM_ADVENTURE_REQUEST_CAPACITY ((loom_u8)4u)
#define LOOM_ADVENTURE_FLAG_NONE ((loom_u8)0xffu)
#define LOOM_ADVENTURE_REQUEST_NONE ((loom_u8)0xffu)

#define LOOM_ADVENTURE_ACTION_NONE ((loom_u8)0u)
#define LOOM_ADVENTURE_ACTION_PICKUP ((loom_u8)1u)
#define LOOM_ADVENTURE_ACTION_INTERACTION ((loom_u8)2u)
/* A collectible (GAME-001): on entry the generated glue adds the trigger's
 * amount to a number variable and queues a pickup request for its sound;
 * the scene keeps it taken until the room is entered again. */
#define LOOM_ADVENTURE_ACTION_COLLECT ((loom_u8)3u)

#define LOOM_ADVENTURE_REQUEST_PICKUP ((loom_u8)1u)
#define LOOM_ADVENTURE_REQUEST_DIALOGUE ((loom_u8)2u)

typedef struct LoomAdventureRequest {
    loom_u8 kind;
    loom_u8 id;
} LoomAdventureRequest;

LOOM_STATIC_ASSERT(loom_adventure_request_is_two_bytes,
                   sizeof(LoomAdventureRequest) == 2u);

LoomStatus loom_adventure_initialize(void);
LoomStatus loom_adventure_flag_set(loom_u8 flag);
LoomStatus loom_adventure_flag_clear(loom_u8 flag);
loom_u8 loom_adventure_flag_is_set(loom_u8 flag);
loom_u8 loom_adventure_gate_allows(loom_u8 flag, loom_u8 required_set);
LoomStatus loom_adventure_try_action(loom_u8 action,
                                     loom_u8 flag,
                                     loom_u8 request_id,
                                     const LoomInputSnapshot *input,
                                     loom_u8 entering,
                                     loom_u8 *dispatched);
LoomStatus loom_adventure_take_request(LoomAdventureRequest *request);
/* Queues a pickup request outside the flag rules, for a collectible that
 * sounds; LOOM_ADVENTURE_REQUEST_NONE queues nothing and succeeds. */
LoomStatus loom_adventure_request_pickup(loom_u8 request_id);
typedef struct LoomAdventureDebugSnapshot {
    loom_u16 flags;
    loom_u16 request_count;
    loom_u8 last_request_kind;
    loom_u8 last_request_id;
    loom_u8 pending_request_count;
    loom_u8 reserved;
} LoomAdventureDebugSnapshot;
void loom_adventure_debug_snapshot(LoomAdventureDebugSnapshot *snapshot);
extern loom_u8 loom_adventure_debug_epoch;
loom_u16 loom_adventure_flags(void);
loom_u16 loom_adventure_request_count(void);
loom_u8 loom_adventure_pending_request_count(void);
loom_u8 loom_adventure_last_request_kind(void);
loom_u8 loom_adventure_last_request_id(void);

#endif
