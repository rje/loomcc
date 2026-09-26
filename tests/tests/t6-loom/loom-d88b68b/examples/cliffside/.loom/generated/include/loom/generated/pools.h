#ifndef LOOM_GENERATED_POOLS_H
#define LOOM_GENERATED_POOLS_H

/* Pool capacities the runtime's fixed arrays are sized from: the actor
 * pool as authored in loom.toml [runtime.pools] and bounded by the
 * play-space profile that owns it, the OAM records per frame commit as
 * the backend capability qualifies them. */
#include <loom/types.h>

#define LOOM_GENERATED_ACTOR_CAPACITY ((loom_u8)32u)
#define LOOM_GENERATED_FRAME_OAM_CAPACITY ((loom_u8)33u)

#endif
