#ifndef LOOM_RASTER_H
#define LOOM_RASTER_H

#include <loom/types.h>

typedef loom_u16 LoomRasterProgramHandle;
typedef loom_u16 LoomRasterStateHandle;

/*
 * Both handles name generated, capability-checked data.  They never expose an
 * HDMA channel, PPU register, table pointer, or backend-specific encoding.
 */
typedef struct LoomRasterBinding {
    LoomRasterProgramHandle program;
    LoomRasterStateHandle state;
} LoomRasterBinding;

#define LOOM_RASTER_PROGRAM_NONE \
    ((LoomRasterProgramHandle)LOOM_INVALID_HANDLE)
#define LOOM_RASTER_STATE_NONE ((LoomRasterStateHandle)LOOM_INVALID_HANDLE)

LOOM_STATIC_ASSERT(loom_raster_binding_is_four_bytes,
                   sizeof(LoomRasterBinding) == 4u);

#endif
