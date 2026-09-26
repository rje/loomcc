#ifndef LOOM_ASSETS_H
#define LOOM_ASSETS_H

#include <loom/types.h>

typedef loom_u16 LoomAssetHandle;

/* A span never crosses the generated segment named by handle. */
typedef struct LoomAssetSpan {
    LoomAssetHandle handle;
    loom_u16 offset;
    loom_u16 length;
} LoomAssetSpan;

LOOM_STATIC_ASSERT(loom_asset_span_is_six_bytes,
                   sizeof(LoomAssetSpan) == 6u);

#endif
