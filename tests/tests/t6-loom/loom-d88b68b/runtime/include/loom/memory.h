#ifndef LOOM_MEMORY_H
#define LOOM_MEMORY_H

#include <loom/types.h>

typedef loom_u16 LoomWramBlockHandle;

/* A span never crosses the statically placed WRAM block named by handle. */
typedef struct LoomWramSpan {
    LoomWramBlockHandle handle;
    loom_u16 offset;
    loom_u16 length;
} LoomWramSpan;

LOOM_STATIC_ASSERT(loom_wram_span_is_six_bytes,
                   sizeof(LoomWramSpan) == 6u);

#endif
