#ifndef LOOM_VIDEO_H
#define LOOM_VIDEO_H

#include <loom/raster.h>
#include <loom/types.h>

#define LOOM_LAYER_BG1 ((loom_u8)0x01u)
#define LOOM_LAYER_BG2 ((loom_u8)0x02u)
#define LOOM_LAYER_BG3 ((loom_u8)0x04u)
#define LOOM_LAYER_BG4 ((loom_u8)0x08u)
#define LOOM_LAYER_OBJ ((loom_u8)0x10u)
#define LOOM_LAYER_BACKDROP ((loom_u8)0x20u)

#define LOOM_DISPLAY_FORCED_BLANK ((loom_u8)0x01u)
#define LOOM_DISPLAY_MODE1_BG3_PRIORITY ((loom_u8)0x02u)

#define LOOM_DISPLAY_MODE_0 ((loom_u8)0u)
#define LOOM_DISPLAY_MODE_1 ((loom_u8)1u)
#define LOOM_DISPLAY_MODE_2 ((loom_u8)2u)
#define LOOM_DISPLAY_MODE_3 ((loom_u8)3u)
#define LOOM_DISPLAY_MODE_4 ((loom_u8)4u)
#define LOOM_DISPLAY_MODE_5 ((loom_u8)5u)
#define LOOM_DISPLAY_MODE_6 ((loom_u8)6u)
#define LOOM_DISPLAY_MODE_7 ((loom_u8)7u)

#define LOOM_OBJ_SIZE_8_16 ((loom_u8)0u)
#define LOOM_OBJ_SIZE_8_32 ((loom_u8)1u)
#define LOOM_OBJ_SIZE_8_64 ((loom_u8)2u)
#define LOOM_OBJ_SIZE_16_32 ((loom_u8)3u)
#define LOOM_OBJ_SIZE_16_64 ((loom_u8)4u)
#define LOOM_OBJ_SIZE_32_64 ((loom_u8)5u)

#define LOOM_COLOR_MATH_SUBTRACT ((loom_u8)0x01u)
#define LOOM_COLOR_MATH_HALF ((loom_u8)0x02u)
#define LOOM_COLOR_MATH_USE_FIXED_COLOR ((loom_u8)0x04u)

#define LOOM_COLOR_BGR555(red, green, blue)                              \
    ((loom_u16)(((loom_u16)(red) & 0x001fu) |                           \
                (((loom_u16)(green) & 0x001fu) << 5) |                  \
                (((loom_u16)(blue) & 0x001fu) << 10)))

#define LOOM_OAM_FLAG_FLIP_X ((loom_u8)0x01u)
#define LOOM_OAM_FLAG_FLIP_Y ((loom_u8)0x02u)
#define LOOM_OAM_SIZE_SMALL ((loom_u8)0u)
#define LOOM_OAM_SIZE_LARGE ((loom_u8)1u)
#define LOOM_OAM_SLOT_MAX ((loom_u8)127u)
#define LOOM_OAM_TILE_INDEX_MAX ((loom_u16)511u)
#define LOOM_OAM_X_MIN ((loom_s16)-256)
#define LOOM_OAM_X_MAX ((loom_s16)255)
#define LOOM_OAM_Y_MIN ((loom_s16)-64)
#define LOOM_OAM_Y_MAX ((loom_s16)255)

#define LOOM_DMA_SOURCE_ROM_ASSET ((loom_u8)0u)
#define LOOM_DMA_SOURCE_WRAM_BLOCK ((loom_u8)1u)
#define LOOM_DMA_DESTINATION_VRAM ((loom_u8)0u)
#define LOOM_DMA_DESTINATION_CGRAM ((loom_u8)1u)
/* VRAM with a 32-word address step per word written: one tilemap column.
 * destination_offset addresses the first word; each following word lands
 * 64 bytes later. */
#define LOOM_DMA_DESTINATION_VRAM_COLUMN ((loom_u8)2u)
#define LOOM_DMA_REQUIRED ((loom_u8)0u)
#define LOOM_DMA_DEFERRABLE ((loom_u8)1u)

#define LOOM_FRAME_COMMIT_FLAGS_NONE ((loom_u8)0u)

#define LOOM_PRESENTATION_NEW_COMMIT ((loom_u8)0u)
#define LOOM_PRESENTATION_REPEATED ((loom_u8)1u)
#define LOOM_PRESENTATION_NONE ((loom_u8)2u)
#define LOOM_COMMIT_NONE ((LoomCommitId)0xffffu)
#define LOOM_MISSED_COMMIT_MAX ((loom_u16)0xffffu)

/*
 * Whole-frame state consumed only at a VBlank commit boundary. Scroll values
 * are signed pixels. Colors are BGR555: R bits 0-4, G 5-9, B 10-14, bit 15
 * zero. brightness is 0..15. mosaic_size is 0 (off) or 2..16 pixels.
 */
typedef struct LoomDisplayState {
    loom_s16 bg_scroll_x[4];
    loom_s16 bg_scroll_y[4];
    loom_u16 backdrop_color;
    loom_u16 fixed_color;
    loom_u8 mode;
    loom_u8 brightness;
    loom_u8 main_layers;
    loom_u8 sub_layers;
    loom_u8 obj_size_pair;
    loom_u8 mosaic_size;
    loom_u8 mosaic_layers;
    loom_u8 color_math_layers;
    loom_u8 color_math_flags;
    loom_u8 flags;
    loom_u8 reserved;
} LoomDisplayState;

/*
 * One logical entry. x/y are signed screen pixels; tile_index is 0..511,
 * palette 0..7, priority 0..3, slot 0..127. The adapter packs the entry into
 * its private OAM shadow and rejects unknown size/flag/reserved values.
 */
typedef struct LoomOamEntry {
    loom_s16 x;
    loom_s16 y;
    loom_u16 tile_index;
    loom_u8 slot;
    loom_u8 palette;
    loom_u8 priority;
    loom_u8 size;
    loom_u8 flags;
    loom_u8 reserved;
} LoomOamEntry;

/*
 * Offsets and lengths are bytes in every domain. source_handle names either a
 * LoomAssetHandle or LoomWramBlockHandle according to source_kind. The adapter
 * chooses physical DMA channels and translates the destination address.
 */
typedef struct LoomDmaJob {
    loom_u16 job_id;
    loom_u16 source_handle;
    loom_u16 source_offset;
    loom_u16 destination_offset;
    loom_u16 byte_count;
    loom_u8 source_kind;
    loom_u8 destination_kind;
    loom_u8 policy;
    loom_u8 reserved;
} LoomDmaJob;

/*
 * The header declares the complete transaction. Entries are copied into
 * adapter-private storage by the staging calls before zero-argument publish.
 */
typedef struct LoomFrameCommit {
    LoomCommitId commit_id;
    LoomDisplayState display;
    LoomRasterBinding raster;
    loom_u8 oam_count;
    loom_u8 dma_count;
    loom_u8 flags;
    loom_u8 reserved;
} LoomFrameCommit;

/*
 * Result of the preceding VBlank plus the identity of the newly latched tick.
 * The first boundary is frame 0, COMMIT_NONE, PRESENTATION_NONE, and zero
 * missed/deferred counts. frame_id wraps modulo 65536; missed count saturates.
 */
typedef struct LoomFrameBoundary {
    LoomFrameId frame_id;
    LoomCommitId presented_commit_id;
    loom_u16 missed_commit_count;
    loom_u8 presentation;
    loom_u8 deferred_dma_jobs;
} LoomFrameBoundary;

LOOM_STATIC_ASSERT(loom_display_state_is_thirty_two_bytes,
                   sizeof(LoomDisplayState) == 32u);
LOOM_STATIC_ASSERT(loom_oam_entry_is_twelve_bytes,
                   sizeof(LoomOamEntry) == 12u);
LOOM_STATIC_ASSERT(loom_dma_job_is_fourteen_bytes,
                   sizeof(LoomDmaJob) == 14u);
LOOM_STATIC_ASSERT(loom_frame_commit_is_forty_two_bytes,
                   sizeof(LoomFrameCommit) == 42u);
LOOM_STATIC_ASSERT(loom_frame_boundary_is_eight_bytes,
                   sizeof(LoomFrameBoundary) == 8u);

#endif
