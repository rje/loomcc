#ifndef LOOM_PVSNESLIB_GENERATED_H
#define LOOM_PVSNESLIB_GENERATED_H

#include <loom/assets.h>
#include <loom/audio.h>
#include <loom/raster.h>

/* Generated adapter-private physical LoROM chunks. */
typedef struct LoomPvsGeneratedAssetChunk {
    LoomAssetHandle handle;
    loom_u16 segment_offset;
    loom_u16 byte_length;
    loom_u16 address;
    loom_u16 bank;
} LoomPvsGeneratedAssetChunk;

LOOM_STATIC_ASSERT(loom_pvs_generated_asset_chunk_is_ten_bytes,
                   sizeof(LoomPvsGeneratedAssetChunk) == 10u);

extern const loom_u16 loom_pvs_generated_asset_chunk_count;
/* VBlanks per logical tick (1 = 60 Hz logic, 2 = 30 Hz), from loom.toml. */
extern const loom_u8 loom_pvs_generated_tick_frames;
extern const LoomPvsGeneratedAssetChunk
    loom_pvs_generated_asset_chunks[];

/* Generated adapter-private cue payload and converter-slot lowering. */
typedef struct LoomPvsGeneratedAudioCue {
    LoomAudioCueHandle handle;
    LoomAssetHandle payload_handle;
    loom_u16 byte_length;
    loom_u16 backend_slot;
    loom_u8 kind;
    loom_u8 reserved;
} LoomPvsGeneratedAudioCue;

LOOM_STATIC_ASSERT(loom_pvs_generated_audio_cue_is_ten_bytes,
                   sizeof(LoomPvsGeneratedAudioCue) == 10u);

extern const loom_u16 loom_pvs_generated_audio_cue_count;
extern const LoomPvsGeneratedAudioCue loom_pvs_generated_audio_cues[];

/* Generated adapter-private physical ownership for a semantic raster handle.
 * ROM kinds play `table_bytes` at `table_offset` of the shared table; the
 * scroll bands read the runtime's WRAM table; the wave's WRAM control table
 * points into `data_bytes` of sine offsets at `data_offset`. */
#define LOOM_PVS_RASTER_KIND_NONE ((loom_u8)0u)
#define LOOM_PVS_RASTER_KIND_FIXED_COLOR ((loom_u8)1u)
#define LOOM_PVS_RASTER_KIND_BACKDROP_GRADIENT ((loom_u8)2u)
#define LOOM_PVS_RASTER_KIND_SCROLL_BANDS ((loom_u8)3u)
#define LOOM_PVS_RASTER_KIND_WAVE ((loom_u8)4u)

typedef struct LoomPvsGeneratedRasterProgram {
    LoomRasterProgramHandle handle;
    loom_u16 table_bytes;
    loom_u8 channel;
    loom_u8 channel_mask;
    loom_u8 transfer_mode;
    loom_u8 target_register;
    loom_u8 kind;
    loom_u8 indirect;
    loom_u16 table_offset;
    loom_u16 data_offset;
    loom_u16 data_bytes;
} LoomPvsGeneratedRasterProgram;

LOOM_STATIC_ASSERT(loom_pvs_generated_raster_program_is_sixteen_bytes,
                   sizeof(LoomPvsGeneratedRasterProgram) == 16u);

extern const loom_u16 loom_pvs_generated_raster_program_count;
extern const LoomPvsGeneratedRasterProgram
    loom_pvs_generated_raster_programs[];
extern const loom_u8 loom_pvs_generated_raster_table_start[];

#endif
