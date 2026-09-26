#include <snes.h>

#include <loom-pvsneslib/frame-transaction.h>
#include <loom-pvsneslib/generated.h>
#include <loom/mode1.h>
#include <loom/pools.h>
#include <loom/port.h>
#include <loom/runtime.h>
#include <loom/save.h>
#if LOOM_SURFACES_ENABLED
#include <loom/surface.h>
#endif
#include <loom/ui.h>

#ifndef LOOM_GENERATED_UI_BACKGROUND
#define LOOM_GENERATED_UI_BACKGROUND 2
#endif

#define LOOM_PVS_DMA_BOUNCE_BYTES LOOM_FRAME_REQUIRED_DMA_BYTES_MAX
#define LOOM_PVS_MODE1_BG1_GFX_WORD_ADDRESS ((loom_u16)0x1000u)
#define LOOM_PVS_MODE1_BG1_MAP_WORD_ADDRESS ((loom_u16)0x2000u)
#define LOOM_PVS_MODE1_BG2_GFX_WORD_ADDRESS ((loom_u16)0x3000u)
#define LOOM_PVS_MODE1_BG2_MAP_WORD_ADDRESS ((loom_u16)0x4000u)
/* BG3 holds the cartridge UI when the project puts it there. Generation
 * writes the byte addresses; the PPU wants them in words. */
#define LOOM_PVS_MODE1_BG3_GFX_WORD_ADDRESS \
    ((loom_u16)(LOOM_UI_FONT_VRAM_BYTE_ADDRESS >> 1))
#define LOOM_PVS_MODE1_BG3_MAP_WORD_ADDRESS \
    ((loom_u16)(LOOM_UI_MAP_VRAM_BYTE_ADDRESS >> 1))
/* A BG3 gameplay layer (the UI on BG2) keeps its four-colour tiles at byte
 * 0xa000 and its map at 0xc000, the regions a BG3 UI would take. */
#define LOOM_PVS_MODE1_BG3_GAMEPLAY_GFX_WORD_ADDRESS ((loom_u16)0x5000u)
#define LOOM_PVS_MODE1_BG3_GAMEPLAY_MAP_WORD_ADDRESS ((loom_u16)0x6000u)
#define LOOM_PVS_AUDIO_QUEUE_CAPACITY ((loom_u8)3u)
#define LOOM_PVS_AUDIO_EFFECT_PITCH ((loom_u8)4u)
#define LOOM_PVS_RASTER_CHANNEL ((loom_u8)6u)
#define LOOM_PVS_RASTER_CHANNEL_MASK ((loom_u8)HDMA_CHANNEL6)
#define LOOM_PVS_RASTER_TRANSFER_MODE ((loom_u8)2u)
#define LOOM_PVS_RASTER_TARGET_REGISTER ((loom_u8)0x32u)
#define LOOM_PVS_RASTER_TABLE_BYTES ((loom_u16)13u)
/* Channel 6's indirect data bank register, which pvsneslib does not name. */
#define LOOM_PVS_REG_DASB6 (*(volatile loom_u8 *)0x4367)
/* The wave's WRAM control table: two continuous rows covering 224 lines
 * (127 + 97) and a terminator. */
#define LOOM_PVS_WAVE_ROW_LINES ((loom_u8)127u)
#define LOOM_PVS_WAVE_VISIBLE_LINES ((loom_u16)224u)

typedef union LoomPvsRuntimePointerParts {
    loom_u8 *pointer;
    const loom_u8 *const_pointer;
    struct {
        loom_u16 address;
        loom_u16 bank;
    } parts;
} LoomPvsRuntimePointerParts;

/* 816-tcc's arithmetic helpers use the high half of this direct-page
 * scratch register before the first helper call establishes it. The SDK
 * startup does not initialize that byte pair, so the adapter owns it. */
#if defined(__65816__) && defined(__TINYC__)
extern volatile loom_u16 tcc__r5h;
#endif

static loom_u8 loom_pvs_runtime_initialized;
static volatile loom_u8 loom_pvs_runtime_did_present;
static loom_u8 loom_pvs_runtime_has_presentation;
static loom_u8 loom_pvs_runtime_has_boundary;
static loom_u8 loom_pvs_runtime_last_presentation;
static loom_u16 loom_pvs_runtime_missed_commit_count;
/* pvsneslib's NMI handler counts VBlanks; the tick owns tick_frames of them
 * counted from the value recorded when it started. */
extern u16 snes_vblank_count;
static loom_u16 loom_pvs_runtime_tick_start_vblanks;
static LoomFrameId loom_pvs_runtime_frame_id;
static LoomCommitId loom_pvs_runtime_presented_commit_id;
/* Not static: vblank.asm folds the pads into these on the console. */
loom_u16 loom_pvs_runtime_input_held[LOOM_INPUT_PAD_CAPACITY];
loom_u16 loom_pvs_runtime_input_pressed[LOOM_INPUT_PAD_CAPACITY];
loom_u16 loom_pvs_runtime_input_released[LOOM_INPUT_PAD_CAPACITY];
static loom_u8 loom_pvs_runtime_dma_bounce[LOOM_PVS_DMA_BOUNCE_BYTES];
static LoomAudioCueCommand
    loom_pvs_runtime_audio_queue[LOOM_PVS_AUDIO_QUEUE_CAPACITY];
static loom_u8 loom_pvs_runtime_audio_count;
static loom_u8 loom_pvs_runtime_audio_enabled;
static loom_u8 loom_pvs_runtime_music_loaded;
static loom_u8 loom_pvs_runtime_music_playing;
static LoomAudioCueHandle loom_pvs_runtime_loaded_music;
static brrsamples loom_pvs_runtime_effect_descriptor;

/* The generated chunk table is sorted by handle, then segment offset. The
 * first chunk of each small handle is cached at init so per-frame lookups
 * start there instead of scanning the whole table. */
#define LOOM_PVS_CHUNK_FIRST_CAPACITY ((loom_u16)128u)
static loom_u16 loom_pvs_runtime_chunk_first[LOOM_PVS_CHUNK_FIRST_CAPACITY];

static void loom_pvs_runtime_index_chunks(void)
{
    loom_u16 index;

    for (index = 0u; index < LOOM_PVS_CHUNK_FIRST_CAPACITY; ++index) {
        loom_pvs_runtime_chunk_first[index] = 0u;
    }
    for (index = loom_pvs_generated_asset_chunk_count; index != 0u;
         --index) {
        LoomAssetHandle handle;

        handle = loom_pvs_generated_asset_chunks[index - 1u].handle;
        if (handle < LOOM_PVS_CHUNK_FIRST_CAPACITY) {
            loom_pvs_runtime_chunk_first[handle] = (loom_u16)(index - 1u);
        }
    }
}

static loom_u8 loom_pvs_runtime_asset_chunk(
    LoomAssetHandle handle,
    loom_u16 offset,
    const LoomPvsGeneratedAssetChunk **chunk,
    loom_u16 *chunk_offset,
    loom_u16 *available)
{
    loom_u16 index;

    index = handle < LOOM_PVS_CHUNK_FIRST_CAPACITY
                ? loom_pvs_runtime_chunk_first[handle]
                : 0u;
    for (; index < loom_pvs_generated_asset_chunk_count; ++index) {
        const LoomPvsGeneratedAssetChunk *candidate;
        loom_u16 relative;

        candidate = &loom_pvs_generated_asset_chunks[index];
        if (candidate->handle > handle) {
            break;
        }
        if (candidate->handle != handle ||
            offset < candidate->segment_offset) {
            continue;
        }
        relative = (loom_u16)(offset - candidate->segment_offset);
        if (relative >= candidate->byte_length) {
            continue;
        }
        *chunk = candidate;
        *chunk_offset = relative;
        *available = (loom_u16)(candidate->byte_length - relative);
        return LOOM_TRUE;
    }
    return LOOM_FALSE;
}

static loom_u8 loom_pvs_runtime_asset_span_valid(
    LoomAssetHandle handle,
    loom_u16 offset,
    loom_u16 length)
{
    loom_u16 remaining;

    remaining = length;
    while (remaining != 0u) {
        const LoomPvsGeneratedAssetChunk *chunk;
        loom_u16 chunk_offset;
        loom_u16 available;
        loom_u16 consumed;

        if (loom_pvs_runtime_asset_chunk(handle, offset, &chunk,
                                         &chunk_offset,
                                         &available) == LOOM_FALSE) {
            return LOOM_FALSE;
        }
        (void)chunk;
        (void)chunk_offset;
        consumed = remaining < available ? remaining : available;
        remaining = (loom_u16)(remaining - consumed);
        offset = (loom_u16)(offset + consumed);
    }
    return LOOM_TRUE;
}

static loom_u8 loom_pvs_runtime_asset_handle_exists(
    LoomAssetHandle handle)
{
    loom_u16 index;

    for (index = 0u; index < loom_pvs_generated_asset_chunk_count;
         ++index) {
        if (loom_pvs_generated_asset_chunks[index].handle == handle) {
            return LOOM_TRUE;
        }
    }
    return LOOM_FALSE;
}

static loom_u16 loom_pvs_runtime_asset_bytes(LoomAssetHandle handle)
{
    loom_u16 index;
    loom_u16 bytes;

    bytes = 0u;
    for (index = 0u; index < loom_pvs_generated_asset_chunk_count;
         ++index) {
        const LoomPvsGeneratedAssetChunk *chunk;
        loom_u16 end;

        chunk = &loom_pvs_generated_asset_chunks[index];
        if (chunk->handle != handle) {
            continue;
        }
        end = (loom_u16)(chunk->segment_offset + chunk->byte_length);
        if (end > bytes) {
            bytes = end;
        }
    }
    return bytes;
}

static loom_u8 *loom_pvs_runtime_chunk_pointer(
    const LoomPvsGeneratedAssetChunk *chunk,
    loom_u16 chunk_offset)
{
    LoomPvsRuntimePointerParts pointer;

    pointer.parts.address = (loom_u16)(chunk->address + chunk_offset);
    pointer.parts.bank = chunk->bank;
    return pointer.pointer;
}

static const LoomPvsGeneratedAudioCue *loom_pvs_runtime_audio_cue(
    LoomAudioCueHandle handle)
{
    const LoomPvsGeneratedAudioCue *cue;
    loom_u16 remaining;

    cue = loom_pvs_generated_audio_cues;
    remaining = loom_pvs_generated_audio_cue_count;
    while (remaining != 0u) {
        if (cue->handle == handle) {
            return cue;
        }
        ++cue;
        --remaining;
    }
    return (const LoomPvsGeneratedAudioCue *)0;
}

static const LoomPvsGeneratedRasterProgram *
loom_pvs_runtime_raster_program(LoomRasterProgramHandle handle)
{
    const LoomPvsGeneratedRasterProgram *program;
    loom_u16 remaining;

    program = loom_pvs_generated_raster_programs;
    remaining = loom_pvs_generated_raster_program_count;
    while (remaining != 0u) {
        if (program->handle == handle) {
            return program;
        }
        ++program;
        --remaining;
    }
    return (const LoomPvsGeneratedRasterProgram *)0;
}

static loom_u8 loom_pvs_runtime_raster_valid(
    const LoomFrameCommit *commit)
{
    const LoomPvsGeneratedRasterProgram *program;

    if (commit->raster.program == LOOM_RASTER_PROGRAM_NONE) {
        return commit->raster.state == LOOM_RASTER_STATE_NONE;
    }
    if (commit->raster.state != LOOM_RASTER_STATE_NONE) {
        return LOOM_FALSE;
    }
    program = loom_pvs_runtime_raster_program(commit->raster.program);
    if (program == (const LoomPvsGeneratedRasterProgram *)0 ||
        program->channel != LOOM_PVS_RASTER_CHANNEL ||
        program->channel_mask != LOOM_PVS_RASTER_CHANNEL_MASK) {
        return LOOM_FALSE;
    }
    switch (program->kind) {
    case LOOM_PVS_RASTER_KIND_FIXED_COLOR:
        if (program->table_bytes != LOOM_PVS_RASTER_TABLE_BYTES ||
            program->transfer_mode != LOOM_PVS_RASTER_TRANSFER_MODE ||
            program->target_register != LOOM_PVS_RASTER_TARGET_REGISTER ||
            commit->raster.state != LOOM_RASTER_STATE_NONE) {
            return LOOM_FALSE;
        }
        return (loom_u8)(commit->display.color_math_layers == LOOM_LAYER_BG1 &&
                         commit->display.color_math_flags ==
                             LOOM_COLOR_MATH_USE_FIXED_COLOR);
    case LOOM_PVS_RASTER_KIND_BACKDROP_GRADIENT:
        return (loom_u8)(program->transfer_mode == 3u &&
                         program->target_register == 0x21u &&
                         program->table_bytes >= 6u &&
                         commit->raster.state == LOOM_RASTER_STATE_NONE);
    case LOOM_PVS_RASTER_KIND_SCROLL_BANDS:
        /* The state names the runtime's buffer; the register is a
         * background's horizontal scroll. */
        return (loom_u8)(program->transfer_mode == 2u &&
                         program->indirect == 0u &&
                         (program->target_register == 0x0du ||
                          program->target_register == 0x0fu ||
                          program->target_register == 0x11u) &&
                         program->table_bytes <= LOOM_MODE1_RASTER_TABLE_BYTES &&
                         commit->raster.state <= 1u);
    case LOOM_PVS_RASTER_KIND_WAVE:
        /* The state is the phase in scanlines, inside one wavelength of
         * data beyond the screen. */
        return (loom_u8)(program->transfer_mode == 2u &&
                         program->indirect != 0u &&
                         (program->target_register == 0x0du ||
                          program->target_register == 0x0fu ||
                          program->target_register == 0x11u) &&
                         program->data_bytes > LOOM_PVS_WAVE_VISIBLE_LINES * 2u &&
                         (loom_u16)(commit->raster.state + LOOM_PVS_WAVE_VISIBLE_LINES) * 2u <=
                             program->data_bytes);
    default:
        return LOOM_FALSE;
    }
}

/* Resolves a cue's whole payload to one chunk pointer. The answer is a flag
 * plus an out-pointer rather than a nullable pointer: a HiROM chunk at a
 * bank's first byte has address $0000, and 816-tcc compares a far pointer
 * with NULL on its low sixteen bits, so that pointer reads as NULL. */
static loom_u8 loom_pvs_runtime_audio_payload(
    const LoomPvsGeneratedAudioCue *cue,
    loom_u8 **payload)
{
    const LoomPvsGeneratedAssetChunk *chunk;
    loom_u16 chunk_offset;
    loom_u16 available;

    if (cue->payload_handle == LOOM_INVALID_HANDLE ||
        cue->byte_length == 0u ||
        loom_pvs_runtime_asset_bytes(cue->payload_handle) !=
            cue->byte_length ||
        loom_pvs_runtime_asset_chunk(cue->payload_handle, 0u, &chunk,
                                     &chunk_offset,
                                     &available) == LOOM_FALSE ||
        chunk_offset != 0u || available < cue->byte_length) {
        return LOOM_FALSE;
    }
    *payload = loom_pvs_runtime_chunk_pointer(chunk, 0u);
    return LOOM_TRUE;
}

static LoomStatus loom_pvs_runtime_audio_initialize(void)
{
    const LoomPvsGeneratedAudioCue *cue;
    const LoomPvsGeneratedAudioCue *music;
    const LoomPvsGeneratedAudioCue *effect;
    loom_u16 remaining;
    loom_u8 *payload;
    loom_u8 sound_region_blocks;

    loom_pvs_runtime_audio_count = 0u;
    loom_pvs_runtime_audio_enabled = LOOM_FALSE;
    loom_pvs_runtime_music_loaded = LOOM_FALSE;
    loom_pvs_runtime_music_playing = LOOM_FALSE;
    loom_pvs_runtime_loaded_music = LOOM_INVALID_HANDLE;
    if (loom_generated_audio_enabled == LOOM_FALSE) {
        return loom_pvs_generated_audio_cue_count == 0u
                   ? LOOM_STATUS_OK
                   : LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_pvs_generated_audio_cue_count == 0u ||
        loom_pvs_generated_audio_cue_count !=
            loom_generated_audio_cue_count) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    music = (const LoomPvsGeneratedAudioCue *)0;
    effect = (const LoomPvsGeneratedAudioCue *)0;
    cue = loom_pvs_generated_audio_cues;
    remaining = loom_pvs_generated_audio_cue_count;
    while (remaining != 0u) {
        if (cue->handle == LOOM_INVALID_HANDLE ||
            loom_pvs_runtime_audio_payload(cue, &payload) == LOOM_FALSE ||
            cue->reserved != 0u ||
            cue->kind > LOOM_AUDIO_CUE_KIND_EFFECT) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (cue->kind == LOOM_AUDIO_CUE_KIND_MUSIC) {
            if (music != (const LoomPvsGeneratedAudioCue *)0) {
                return LOOM_STATUS_UNSUPPORTED;
            }
            music = cue;
        } else {
            /* Effects share one streaming voice; the sound region holds
             * the largest, and each play points the descriptor at its cue. */
            if (cue->backend_slot > 255u || cue->byte_length > 0xff00u) {
                return LOOM_STATUS_UNSUPPORTED;
            }
            if (effect == (const LoomPvsGeneratedAudioCue *)0 ||
                cue->byte_length > effect->byte_length) {
                effect = cue;
            }
        }
        ++cue;
        --remaining;
    }

    spcBoot();
    sound_region_blocks = 0u;
    if (effect != (const LoomPvsGeneratedAudioCue *)0) {
        sound_region_blocks = (loom_u8)(effect->byte_length >> 8);
        if ((effect->byte_length & 0x00ffu) != 0u) {
            ++sound_region_blocks;
        }
    }
    spcAllocateSoundRegion((u8)sound_region_blocks);
    if (effect != (const LoomPvsGeneratedAudioCue *)0) {
        (void)loom_pvs_runtime_audio_payload(effect, &payload);
        spcSetSoundEntry(15u,
                         8u,
                         LOOM_PVS_AUDIO_EFFECT_PITCH,
                         (u16)effect->byte_length,
                         (u8 *)payload,
                         &loom_pvs_runtime_effect_descriptor);
    }
    if (music != (const LoomPvsGeneratedAudioCue *)0) {
        (void)loom_pvs_runtime_audio_payload(music, &payload);
        spcSetBank((u8 *)payload);
    }
    loom_pvs_runtime_audio_enabled = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

static void loom_pvs_runtime_copy_asset(LoomAssetHandle handle,
                                        loom_u16 offset,
                                        loom_u8 *destination,
                                        loom_u16 length)
{
    loom_u16 remaining;

    remaining = length;
    while (remaining != 0u) {
        const LoomPvsGeneratedAssetChunk *chunk;
        loom_u16 chunk_offset;
        loom_u16 available;
        loom_u16 copied;
        loom_u16 index;
        loom_u8 *source;

        (void)loom_pvs_runtime_asset_chunk(handle, offset, &chunk,
                                           &chunk_offset, &available);
        copied = remaining < available ? remaining : available;
        source = loom_pvs_runtime_chunk_pointer(chunk, chunk_offset);
        for (index = 0u; index < copied; ++index) {
            destination[index] = source[index];
        }
        destination += copied;
        offset = (loom_u16)(offset + copied);
        remaining = (loom_u16)(remaining - copied);
    }
}

/* VMAIN 0x81: step 32 words after each high byte; DMA mode 1 into $2118. */
#define LOOM_PVS_VRAM_COLUMN_VMAIN ((u8)0x81u)
#define LOOM_PVS_VRAM_COLUMN_DMA_CONTROL ((u16)0x1801u)

static void loom_pvs_runtime_dma_copy(const LoomDmaJob *job,
                                      loom_u8 *source)
{
    if (job->destination_kind == LOOM_DMA_DESTINATION_VRAM) {
        dmaCopyVram((u8 *)source,
                    (u16)(job->destination_offset >> 1),
                    (u16)job->byte_count);
    } else if (job->destination_kind == LOOM_DMA_DESTINATION_VRAM_COLUMN) {
        dmaCopyVram7((u8 *)source,
                     (u16)(job->destination_offset >> 1),
                     (u16)job->byte_count,
                     LOOM_PVS_VRAM_COLUMN_VMAIN,
                     LOOM_PVS_VRAM_COLUMN_DMA_CONTROL);
    } else {
        dmaCopyCGram((u8 *)source,
                     (u16)(job->destination_offset >> 1),
                     (u16)job->byte_count);
    }
}

static loom_u8 *loom_pvs_runtime_prepare_dma_source(
    const LoomDmaJob *job,
    loom_u16 *bounce_offset)
{
    const LoomPvsGeneratedAssetChunk *chunk;
    loom_u16 chunk_offset;
    loom_u16 available;
    loom_u8 *source;

    if (job->source_kind == LOOM_DMA_SOURCE_WRAM_BLOCK) {
        loom_u16 block_bytes;

        /* WRAM blocks need no bounce: the stream block, a surface shadow or
         * a UI number's composed digits. */
#if LOOM_SURFACES_ENABLED
        if (job->source_handle == LOOM_SURFACE_BLOCK_HANDLE) {
            return loom_surface_block(&block_bytes) + job->source_offset;
        }
#endif
#if LOOM_GENERATED_PROJECT_UI_ENABLED
        if (job->source_handle == LOOM_UI_BLOCK_HANDLE) {
            return (loom_u8 *)loom_ui_block(&block_bytes) + job->source_offset;
        }
#endif
        return loom_mode1_stream_block(&block_bytes) + job->source_offset;
    }
    (void)loom_pvs_runtime_asset_chunk(job->source_handle,
                                       job->source_offset,
                                       &chunk,
                                       &chunk_offset,
                                       &available);
    if (available >= job->byte_count) {
        return loom_pvs_runtime_chunk_pointer(chunk, chunk_offset);
    }
    source = &loom_pvs_runtime_dma_bounce[*bounce_offset];
    loom_pvs_runtime_copy_asset(job->source_handle,
                                job->source_offset,
                                source,
                                job->byte_count);
    *bounce_offset = (loom_u16)(*bounce_offset + job->byte_count);
    return source;
}

#if defined(__65816__)
/* vblank.asm: the same register writes without 816-tcc's fourteen
 * scanlines of masking and shifting. */
void loom_pvs_display_apply(const LoomDisplayState *display);
void loom_pvs_dma_run(const LoomDmaJob *jobs, loom_u8 **sources, loom_u16 count);
void loom_pvs_inputs_sample(void);
#define loom_pvs_runtime_apply_display(display) loom_pvs_display_apply(display)
#define loom_pvs_runtime_sample_inputs() loom_pvs_inputs_sample()
#else
static void loom_pvs_runtime_apply_display(
    const LoomDisplayState *display)
{
    loom_u8 color_math;
    loom_u8 inidisp;

    REG_BGMODE = (u8)(display->mode |
                      ((display->flags &
                        LOOM_DISPLAY_MODE1_BG3_PRIORITY) != 0u
                           ? 0x08u
                           : 0u));
    if (display->mosaic_size == 0u) {
        REG_MOSAIC = 0u;
    } else {
        REG_MOSAIC =
            (u8)((u8)((display->mosaic_size - 1u) << 4) |
                 (display->mosaic_layers & 0x0fu));
    }

    REG_BG1HOFS = (u8)((loom_u16)display->bg_scroll_x[0] & 0x00ffu);
    REG_BG1HOFS =
        (u8)(((loom_u16)display->bg_scroll_x[0] >> 8) & 0x00ffu);
    REG_BG1VOFS = (u8)((loom_u16)display->bg_scroll_y[0] & 0x00ffu);
    REG_BG1VOFS =
        (u8)(((loom_u16)display->bg_scroll_y[0] >> 8) & 0x00ffu);
    REG_BG2HOFS = (u8)((loom_u16)display->bg_scroll_x[1] & 0x00ffu);
    REG_BG2HOFS =
        (u8)(((loom_u16)display->bg_scroll_x[1] >> 8) & 0x00ffu);
    REG_BG2VOFS = (u8)((loom_u16)display->bg_scroll_y[1] & 0x00ffu);
    REG_BG2VOFS =
        (u8)(((loom_u16)display->bg_scroll_y[1] >> 8) & 0x00ffu);
    REG_BG3HOFS = (u8)((loom_u16)display->bg_scroll_x[2] & 0x00ffu);
    REG_BG3HOFS =
        (u8)(((loom_u16)display->bg_scroll_x[2] >> 8) & 0x00ffu);
    REG_BG3VOFS = (u8)((loom_u16)display->bg_scroll_y[2] & 0x00ffu);
    REG_BG3VOFS =
        (u8)(((loom_u16)display->bg_scroll_y[2] >> 8) & 0x00ffu);
    REG_BG4HOFS = (u8)((loom_u16)display->bg_scroll_x[3] & 0x00ffu);
    REG_BG4HOFS =
        (u8)(((loom_u16)display->bg_scroll_x[3] >> 8) & 0x00ffu);
    REG_BG4VOFS = (u8)((loom_u16)display->bg_scroll_y[3] & 0x00ffu);
    REG_BG4VOFS =
        (u8)(((loom_u16)display->bg_scroll_y[3] >> 8) & 0x00ffu);

    REG_CGADD = 0u;
    *CGRAM_PALETTE = (u8)(display->backdrop_color & 0x00ffu);
    *CGRAM_PALETTE =
        (u8)((display->backdrop_color >> 8) & 0x007fu);
    REG_COLDATA = (u8)(0x20u | (display->fixed_color & 0x001fu));
    REG_COLDATA =
        (u8)(0x40u | ((display->fixed_color >> 5) & 0x001fu));
    REG_COLDATA =
        (u8)(0x80u | ((display->fixed_color >> 10) & 0x001fu));
    REG_TM = (u8)(display->main_layers & 0x1fu);
    REG_TS = (u8)(display->sub_layers & 0x1fu);
    REG_CGWSEL =
        (display->color_math_flags &
         LOOM_COLOR_MATH_USE_FIXED_COLOR) != 0u
            ? 0u
            : 2u;
    color_math = (u8)(display->color_math_layers & 0x3fu);
    if ((display->color_math_flags & LOOM_COLOR_MATH_HALF) != 0u) {
        color_math |= 0x40u;
    }
    if ((display->color_math_flags & LOOM_COLOR_MATH_SUBTRACT) != 0u) {
        color_math |= 0x80u;
    }
    REG_CGADSUB = color_math;
    REG_OBSEL = (u8)((display->obj_size_pair & 0x07u) << 5);

    inidisp = (u8)(display->brightness & 0x0fu);
    if ((display->flags & LOOM_DISPLAY_FORCED_BLANK) != 0u) {
        inidisp |= 0x80u;
    }
    REG_INIDISP = inidisp;
    mirrorINIDISP = inidisp;
}
#endif

static loom_u8 loom_pvs_wave_table[7];

static LoomRasterProgramHandle loom_pvs_runtime_raster_last_handle =
    LOOM_RASTER_PROGRAM_NONE;
static const LoomPvsGeneratedRasterProgram *loom_pvs_runtime_raster_last_program;

static void loom_pvs_runtime_apply_raster(
    const LoomRasterBinding *binding)
{
    const LoomPvsGeneratedRasterProgram *program;
    LoomPvsRuntimePointerParts pointer;
    loom_u16 data_address;

    /* The adapter owns HDMAEN and channel 6 exclusively. Disable before
     * changing the channel registers so replacement and teardown are atomic;
     * this runs every VBlank, so a WRAM table re-points every frame. */
    REG_HDMAEN = 0u;
    if (binding->program == LOOM_RASTER_PROGRAM_NONE) {
        return;
    }
    /* The program changes with the room, not the frame: the last lookup is
     * kept, since the linear search ran every VBlank. */
    if (binding->program != loom_pvs_runtime_raster_last_handle) {
        loom_pvs_runtime_raster_last_program =
            loom_pvs_runtime_raster_program(binding->program);
        loom_pvs_runtime_raster_last_handle = binding->program;
    }
    program = loom_pvs_runtime_raster_last_program;
    if (program == (const LoomPvsGeneratedRasterProgram *)0) {
        return;
    }
    if (program->kind == LOOM_PVS_RASTER_KIND_SCROLL_BANDS) {
        pointer.pointer = loom_mode1_raster_tables[binding->state & 1u];
        REG_DMAP6 = program->transfer_mode;
    } else if (program->kind == LOOM_PVS_RASTER_KIND_WAVE) {
        /* Two continuous rows whose data pointers start `state` lines into
         * the sine table; indirect mode reads the data from that bank. */
        pointer.const_pointer = loom_pvs_generated_raster_table_start;
        data_address = (loom_u16)(pointer.parts.address + program->data_offset +
                                  (loom_u16)(binding->state * 2u));
        loom_pvs_wave_table[0] = (loom_u8)(0x80u | LOOM_PVS_WAVE_ROW_LINES);
        loom_pvs_wave_table[1] = (loom_u8)(data_address & 0x00ffu);
        loom_pvs_wave_table[2] = (loom_u8)((data_address >> 8) & 0x00ffu);
        data_address = (loom_u16)(data_address + (loom_u16)LOOM_PVS_WAVE_ROW_LINES * 2u);
        loom_pvs_wave_table[3] =
            (loom_u8)(0x80u | (loom_u8)(LOOM_PVS_WAVE_VISIBLE_LINES - LOOM_PVS_WAVE_ROW_LINES));
        loom_pvs_wave_table[4] = (loom_u8)(data_address & 0x00ffu);
        loom_pvs_wave_table[5] = (loom_u8)((data_address >> 8) & 0x00ffu);
        loom_pvs_wave_table[6] = 0u;
        LOOM_PVS_REG_DASB6 = (loom_u8)(pointer.parts.bank & 0x00ffu);
        pointer.pointer = loom_pvs_wave_table;
        REG_DMAP6 = (u8)(0x40u | program->transfer_mode);
    } else {
        pointer.const_pointer =
            loom_pvs_generated_raster_table_start + program->table_offset;
        REG_DMAP6 = program->transfer_mode;
    }
    REG_BBAD6 = program->target_register;
    REG_A1T6LH = (u16)pointer.parts.address;
    REG_A1B6 = (u8)(pointer.parts.bank & 0x00ffu);
    REG_HDMAEN = LOOM_PVS_RASTER_CHANNEL_MASK;
}

#if !defined(__65816__)
static void loom_pvs_runtime_sample_inputs(void)
{
    loom_u8 pad;

    while ((REG_HVBJOY & 0x01u) != 0u) {
    }
    for (pad = 0u; pad < LOOM_INPUT_PAD_CAPACITY; ++pad) {
        loom_u16 current;
        loom_u16 previous;

        current = (loom_u16)REG_JOYxLH(pad);
        /* A standard pad answers 0000 in its low nibble; an empty port (or
         * a mouse) does not, and must not read as every button held. */
        if ((current & 0x000fu) != 0u) {
            current = 0u;
        }
        previous = loom_pvs_runtime_input_held[pad];
        loom_pvs_runtime_input_pressed[pad] |=
            (loom_u16)(current & (loom_u16)(~previous));
        loom_pvs_runtime_input_released[pad] |=
            (loom_u16)(previous & (loom_u16)(~current));
        loom_pvs_runtime_input_held[pad] = current;
    }
}
#endif

#if defined(LOOM_BUILD_DEBUG)
/*
 * Tick timing witnesses for debug ROMs. Every traced phase boundary (and
 * profiling mark) records the scanline and the VBlank count; when the tick
 * ends the adapter turns the log into per-phase costs in scanlines, so a
 * watch always reads one consistent picture of the last completed tick.
 * NTSC has 262 scanlines per frame.
 */
#define LOOM_PVS_DEBUG_EVENT_CAPACITY ((loom_u8)24u)
#define LOOM_PVS_DEBUG_LINES_PER_FRAME ((loom_u16)262u)
#define LOOM_PVS_DEBUG_NMI_LINE ((loom_u16)225u)
#define LOOM_PVS_DEBUG_EVENT_TICK_START ((loom_u8)0u)
#define LOOM_PVS_DEBUG_EVENT_TICK_END ((loom_u8)0xffu)
#define LOOM_PVS_DEBUG_EVENT_MARK_BASE ((loom_u8)16u)
volatile loom_u16 loom_pvs_debug_tick_start_line;
volatile loom_u16 loom_pvs_debug_tick_end_line;
volatile loom_u16 loom_pvs_debug_tick_cost_lines;
volatile loom_u16 loom_pvs_debug_tick_cost_max_lines;
volatile loom_u16 loom_pvs_debug_tick_lag_frames;
volatile loom_u16 loom_pvs_debug_tick_lag_total;
/* Scanlines each phase of the last tick took, from its trace point to the
 * next one, indexed by the phase's trace id: 0 is frame_shell (the glue
 * before the first phase), then read_actions 1, update_behaviors 2,
 * move_and_collide 3, dispatch_triggers 4, advance_animation 5,
 * build_render 6, process_audio 7, submit_frame 8, debug_witness 9; the
 * profiling marks store at 16 plus their id. One indexed store at each
 * boundary, where a switch over named variables cost 816-tcc a scanline
 * and a half per phase. */
volatile loom_u16 loom_pvs_debug_phase_cost[24];
/* How many trace events the last tick recorded. Eleven is one per
 * phase boundary plus the tick's own two; more means a phase was
 * entered twice and its earlier segment is missing from the table. */
volatile loom_u16 loom_pvs_debug_tick_events;
/* Where the frame wait began and ended: the raster line and VBlank count
 * on entering WaitForVBlank and on leaving it, so a wait that spans two
 * VBlanks shows as such. */
volatile loom_u16 loom_pvs_debug_wait_line;
volatile loom_u16 loom_pvs_debug_wait_vblanks;
volatile loom_u16 loom_pvs_debug_wake_line;
volatile loom_u16 loom_pvs_debug_wake_vblanks;
/* Raster lines after the frame's DMA copies, after the display and raster
 * registers are applied, and after the pads are sampled: the VBlank work
 * between waking and the next tick. */
volatile loom_u16 loom_pvs_debug_dma_done_line;
volatile loom_u16 loom_pvs_debug_present_done_line;
volatile loom_u16 loom_pvs_debug_inputs_done_line;
static loom_u16 loom_pvs_debug_event_line[LOOM_PVS_DEBUG_EVENT_CAPACITY];
static loom_u8 loom_pvs_debug_event_vblank[LOOM_PVS_DEBUG_EVENT_CAPACITY];
static loom_u8 loom_pvs_debug_event_id[LOOM_PVS_DEBUG_EVENT_CAPACITY];
static loom_u8 loom_pvs_debug_event_count;
static u16 loom_pvs_debug_tick_start_vblanks;
static loom_u8 loom_pvs_debug_tick_open;
static volatile u8 loom_pvs_debug_sink;

static loom_u16 loom_pvs_debug_scanline_now(void)
{
    u8 low;
    u8 high;

    /* STAT78 resets the read flip-flops; SLHV latches (WRIO bit 7 was set
     * at init); OPVCT reads low then high. */
    loom_pvs_debug_sink = *(volatile u8 *)0x213fu;
    loom_pvs_debug_sink = *(volatile u8 *)0x2137u;
    low = *(volatile u8 *)0x213du;
    high = *(volatile u8 *)0x213du;
    return (loom_u16)((loom_u16)low | ((loom_u16)(high & 1u) << 8));
}

static void loom_pvs_debug_store_cost(loom_u8 event_id, loom_u16 cost);

/* Each event closes the phase before it: its cost is stored here, at the
 * boundary, as one subtraction. Doing the ten of them in a loop after the
 * tick once took 35 scanlines under 816-tcc -- past the NMI, so the ISR
 * found nobody waiting and every tick took two frames. */
static void loom_pvs_debug_record(loom_u8 event_id)
{
    loom_u8 index;
    loom_u16 line;
    loom_u8 vblank;

    index = loom_pvs_debug_event_count;
    if (index >= LOOM_PVS_DEBUG_EVENT_CAPACITY) {
        return;
    }
    /* Lines are stored relative to the VBlank NMI at line 225, where the
     * VBlank counter steps, so a frame crossing is one counter step. */
    line = loom_pvs_debug_scanline_now();
    line = line >= LOOM_PVS_DEBUG_NMI_LINE
               ? (loom_u16)(line - LOOM_PVS_DEBUG_NMI_LINE)
               : (loom_u16)(line + (LOOM_PVS_DEBUG_LINES_PER_FRAME -
                                    LOOM_PVS_DEBUG_NMI_LINE));
    vblank = (loom_u8)snes_vblank_count;
    if (index != 0u) {
        loom_u16 cost;
        loom_u8 frames;

        cost = (loom_u16)(line - loom_pvs_debug_event_line[index - 1u]);
        frames = (loom_u8)(vblank - loom_pvs_debug_event_vblank[index - 1u]);
        while (frames != 0u) {
            cost = (loom_u16)(cost + LOOM_PVS_DEBUG_LINES_PER_FRAME);
            --frames;
        }
        loom_pvs_debug_store_cost(loom_pvs_debug_event_id[index - 1u], cost);
    }
    loom_pvs_debug_event_line[index] = line;
    loom_pvs_debug_event_vblank[index] = vblank;
    loom_pvs_debug_event_id[index] = event_id;
    loom_pvs_debug_event_count = (loom_u8)(index + 1u);
}

void loom_pvs_debug_phase(loom_u8 phase_id)
{
    if (loom_pvs_debug_tick_open != LOOM_FALSE) {
        loom_pvs_debug_record(phase_id);
    }
}

#if defined(LOOM_RUNTIME_PROFILE_MARKS)
void loom_pvs_debug_mark(loom_u8 mark_id)
{
    if (loom_pvs_debug_tick_open != LOOM_FALSE) {
        loom_pvs_debug_record(
            (loom_u8)(LOOM_PVS_DEBUG_EVENT_MARK_BASE + mark_id));
    }
}
#endif

static void loom_pvs_debug_store_cost(loom_u8 event_id, loom_u16 cost)
{
    if (event_id < 24u) {
        loom_pvs_debug_phase_cost[event_id] = cost;
    }
}

/* Scanlines from event `from` to event `to`: the NMI-relative line
 * difference plus a frame for each VBlank crossed. */
static loom_u16 loom_pvs_debug_lines_between(loom_u8 from, loom_u8 to)
{
    loom_u16 cost;
    loom_u8 frames;

    frames = (loom_u8)(loom_pvs_debug_event_vblank[to] -
                       loom_pvs_debug_event_vblank[from]);
    cost = (loom_u16)(loom_pvs_debug_event_line[to] -
                      loom_pvs_debug_event_line[from]);
    while (frames != 0u) {
        cost = (loom_u16)(cost + LOOM_PVS_DEBUG_LINES_PER_FRAME);
        --frames;
    }
    return cost;
}

static void loom_pvs_debug_tick_ended(void)
{
    loom_u16 line;
    loom_u16 vblanks;
    loom_u16 cost;
    loom_u8 last;

    if (loom_pvs_debug_tick_open == LOOM_FALSE) {
        return;
    }
    loom_pvs_debug_record(LOOM_PVS_DEBUG_EVENT_TICK_END);
    loom_pvs_debug_tick_open = LOOM_FALSE;
    line = loom_pvs_debug_scanline_now();
    vblanks = (loom_u16)(snes_vblank_count - loom_pvs_debug_tick_start_vblanks);
    loom_pvs_debug_tick_end_line = line;
    /* The tick owns tick_frames VBlanks; only VBlanks beyond that are lag. */
    loom_pvs_debug_tick_lag_frames =
        vblanks >= loom_pvs_generated_tick_frames
            ? (loom_u16)(vblanks - (loom_pvs_generated_tick_frames - 1u))
            : 0u;
    loom_pvs_debug_tick_lag_total = (loom_u16)(
        loom_pvs_debug_tick_lag_total + loom_pvs_debug_tick_lag_frames);
    /* The whole tick, from the start event to the end event, in the same
     * NMI-relative terms as the phases. */
    /* This bookkeeping runs between the tick's end and WaitForVBlank, in
     * the slack before the NMI. A 16-bit multiply is a library call under
     * 816-tcc, and eleven of them here once cost more than that slack: the
     * ISR then found nobody waiting, counted a lag frame, and every tick
     * took two frames while reporting a cost that fit one. The phases'
     * costs are stored at each boundary now; only the total remains. */
    last = (loom_u8)(loom_pvs_debug_event_count - 1u);
    cost = loom_pvs_debug_lines_between(0u, last);
    loom_pvs_debug_tick_cost_lines = cost;
    loom_pvs_debug_tick_events = loom_pvs_debug_event_count;
    if (cost > loom_pvs_debug_tick_cost_max_lines) {
        loom_pvs_debug_tick_cost_max_lines = cost;
    }
}

static void loom_pvs_debug_tick_started(void)
{
    loom_pvs_debug_event_count = 0u;
    loom_pvs_debug_tick_open = LOOM_TRUE;
    loom_pvs_debug_record(LOOM_PVS_DEBUG_EVENT_TICK_START);
    loom_pvs_debug_tick_start_line = loom_pvs_debug_scanline_now();
    loom_pvs_debug_tick_start_vblanks = snes_vblank_count;
}
#else
#define loom_pvs_debug_tick_ended() ((void)0)
#define loom_pvs_debug_tick_started() ((void)0)
#endif

/* Let one VBlank pass without WaitForVBlank, so the ISR sees a lag frame
 * and leaves the OAM shadow and the pads alone. */
static void loom_pvs_runtime_pass_vblank(void)
{
#if defined(__65816__)
    loom_u16 seen;

    seen = *(volatile u16 *)&snes_vblank_count;
    while (*(volatile u16 *)&snes_vblank_count == seen) {
    }
#else
    /* Host builds have no NMI handler: the fake counter advances by hand. */
    ++snes_vblank_count;
#endif
}

static void loom_pvs_runtime_advance_physical_frame(void)
{
    const LoomFrameCommit *commit;
    const LoomDmaJob *dma;
    loom_u8 *dma_sources[LOOM_FRAME_DMA_CAPACITY];
    loom_u16 bounce_offset;
    loom_u8 dma_count;
    loom_u8 index;
    loom_u8 ready;

    loom_pvs_debug_tick_ended();
    ready = loom_pvs_frame_transaction_ready();
    commit = (const LoomFrameCommit *)0;
    dma = (const LoomDmaJob *)0;
    dma_count = 0u;
    bounce_offset = 0u;
    if (ready != LOOM_FALSE) {
        commit = loom_pvs_frame_transaction_commit();
        dma = loom_pvs_frame_transaction_dma();
        dma_count = loom_pvs_frame_transaction_dma_count();
        for (index = 0u; index < dma_count; ++index) {
            dma_sources[index] = loom_pvs_runtime_prepare_dma_source(
                &dma[index], &bounce_offset);
        }
    }
    /* The tick owns tick_frames VBlanks counted from its start, so the
     * display refreshes on a fixed cadence while the work fits in that
     * window; a tick that overruns costs whole frames, never partial ones.
     * The VBlanks before the last are passed without WaitForVBlank so
     * pvsneslib's ISR treats them as lag frames: it leaves the OAM shadow
     * and the pads alone until the VBlank that presents this commit. */
    while ((loom_u16)(*(volatile u16 *)&snes_vblank_count -
                      loom_pvs_runtime_tick_start_vblanks) <
           (loom_u16)(loom_pvs_generated_tick_frames - 1u)) {
        loom_pvs_runtime_pass_vblank();
    }
#if defined(LOOM_BUILD_DEBUG)
    loom_pvs_debug_wait_line = loom_pvs_debug_scanline_now();
    loom_pvs_debug_wait_vblanks = snes_vblank_count;
#endif
    WaitForVBlank();
#if defined(LOOM_BUILD_DEBUG)
    loom_pvs_debug_wake_line = loom_pvs_debug_scanline_now();
    loom_pvs_debug_wake_vblanks = snes_vblank_count;
#endif
    /* All pointer resolution and bank-crossing copies happen before the
     * fence. Only the bounded hardware transfers remain in VBlank. */
    if (ready != LOOM_FALSE) {
#if defined(__65816__)
        /* vblank.asm programs each job's registers itself: through C each
         * job cost a few scanlines, and a dozen overran VBlank, where the
         * PPU drops VRAM writes. */
        loom_pvs_dma_run(dma, dma_sources, dma_count);
#else
        for (index = 0u; index < dma_count; ++index) {
            loom_pvs_runtime_dma_copy(&dma[index], dma_sources[index]);
        }
#endif
#if defined(LOOM_BUILD_DEBUG)
        loom_pvs_debug_dma_done_line = loom_pvs_debug_scanline_now();
#endif
        loom_pvs_runtime_apply_display(&commit->display);
        loom_pvs_runtime_apply_raster(&commit->raster);
        loom_pvs_runtime_presented_commit_id = commit->commit_id;
        loom_pvs_runtime_has_presentation = LOOM_TRUE;
        loom_pvs_runtime_did_present = LOOM_TRUE;
        loom_pvs_frame_transaction_presented();
    }
#if defined(LOOM_BUILD_DEBUG)
    loom_pvs_debug_present_done_line = loom_pvs_debug_scanline_now();
#endif
    loom_pvs_runtime_tick_start_vblanks = snes_vblank_count;
    loom_pvs_runtime_sample_inputs();
#if defined(LOOM_BUILD_DEBUG)
    loom_pvs_debug_inputs_done_line = loom_pvs_debug_scanline_now();
#endif
    if (loom_pvs_runtime_has_boundary == LOOM_FALSE) {
        loom_pvs_runtime_frame_id = 0u;
        loom_pvs_runtime_last_presentation = LOOM_PRESENTATION_NONE;
        loom_pvs_runtime_has_boundary = LOOM_TRUE;
    } else {
        ++loom_pvs_runtime_frame_id;
        if (loom_pvs_runtime_did_present != LOOM_FALSE) {
            loom_pvs_runtime_last_presentation =
                LOOM_PRESENTATION_NEW_COMMIT;
        } else if (loom_pvs_runtime_has_presentation != LOOM_FALSE) {
            if (loom_pvs_runtime_missed_commit_count !=
                LOOM_MISSED_COMMIT_MAX) {
                ++loom_pvs_runtime_missed_commit_count;
            }
            loom_pvs_runtime_last_presentation =
                LOOM_PRESENTATION_REPEATED;
        } else {
            loom_pvs_runtime_last_presentation =
                LOOM_PRESENTATION_NONE;
        }
    }
    loom_pvs_runtime_did_present = LOOM_FALSE;
    loom_pvs_debug_tick_started();
}

loom_u8 loom_pvs_target_commit_supported(const LoomFrameCommit *commit)
{
    const LoomDisplayState *display;

    display = &commit->display;
    if (display->reserved != 0u || display->mode != LOOM_DISPLAY_MODE_1 ||
        display->brightness > 15u ||
        (display->main_layers & (loom_u8)(~0x1fu)) != 0u ||
        (display->sub_layers & (loom_u8)(~0x1fu)) != 0u ||
        display->obj_size_pair > LOOM_OBJ_SIZE_32_64 ||
        display->mosaic_size > 16u || display->mosaic_size == 1u ||
        (display->mosaic_layers & (loom_u8)(~0x0fu)) != 0u ||
        (display->mosaic_size == 0u && display->mosaic_layers != 0u) ||
        (display->color_math_layers & (loom_u8)(~0x3fu)) != 0u ||
        (display->color_math_flags & (loom_u8)(~0x07u)) != 0u ||
        (display->flags & (loom_u8)(~0x03u)) != 0u ||
        (display->backdrop_color & 0x8000u) != 0u ||
        (display->fixed_color & 0x8000u) != 0u ||
        loom_pvs_runtime_raster_valid(commit) == LOOM_FALSE) {
        return LOOM_FALSE;
    }
    return LOOM_TRUE;
}

loom_u8 loom_pvs_target_dma_source_valid(const LoomDmaJob *job)
{
    if (job->source_kind == LOOM_DMA_SOURCE_WRAM_BLOCK) {
        loom_u16 block_bytes;
        const loom_u8 *block;

        block_bytes = 0u;
        block = (const loom_u8 *)0;
        if (job->source_handle == LOOM_MODE1_STREAM_BLOCK_HANDLE) {
            block = loom_mode1_stream_block(&block_bytes);
        }
#if LOOM_SURFACES_ENABLED
        else if (job->source_handle == LOOM_SURFACE_BLOCK_HANDLE) {
            block = loom_surface_block(&block_bytes);
        }
#endif
#if LOOM_GENERATED_PROJECT_UI_ENABLED
        else if (job->source_handle == LOOM_UI_BLOCK_HANDLE) {
            block = loom_ui_block(&block_bytes);
        }
#endif
        if (block == (const loom_u8 *)0 ||
            job->source_offset > block_bytes ||
            job->byte_count > (loom_u16)(block_bytes - job->source_offset)) {
            return LOOM_FALSE;
        }
        return LOOM_TRUE;
    }
    if (job->source_kind != LOOM_DMA_SOURCE_ROM_ASSET) {
        return LOOM_FALSE;
    }
    return loom_pvs_runtime_asset_span_valid(job->source_handle,
                                             job->source_offset,
                                             job->byte_count);
}

#if !defined(__65816__)
/* Slots written last frame, so only those need hiding when they vanish:
 * pvsneslib's full oamClear costs most of a frame of CPU time. The console
 * runs this path in oam.asm; the host keeps it in C for the contract tests. */
static loom_u8 loom_pvs_runtime_oam_previous[LOOM_FRAME_OAM_CAPACITY];
static loom_u8 loom_pvs_runtime_oam_previous_count;
static loom_u8 loom_pvs_runtime_oam_mark[LOOM_OAM_SLOT_MAX + 1u];

/* OAM high-table bit positions for slot & 3; variable shifts are loops on
 * 816-tcc, a table lookup is one indexed load. */
static const u8 loom_pvs_runtime_oam_high_keep[4] = {0xfcu, 0xf3u, 0xcfu,
                                                     0x3fu};
static const u8 loom_pvs_runtime_oam_high_x[4] = {0x01u, 0x04u, 0x10u, 0x40u};
static const u8 loom_pvs_runtime_oam_high_large[4] = {0x02u, 0x08u, 0x20u,
                                                      0x80u};

LoomStatus loom_pvs_target_prepare_oam(const LoomOamEntry *entries,
                                       loom_u8 entry_count)
{
    const LoomOamEntry *entry;
    loom_u8 *previous;
    loom_u8 remaining;

    entry = entries;
    for (remaining = entry_count; remaining != 0u; --remaining, ++entry) {
        loom_pvs_runtime_oam_mark[entry->slot] = LOOM_TRUE;
    }
    previous = loom_pvs_runtime_oam_previous;
    for (remaining = loom_pvs_runtime_oam_previous_count; remaining != 0u;
         --remaining, ++previous) {
        loom_u8 slot;

        slot = *previous;
        if (loom_pvs_runtime_oam_mark[slot] == LOOM_FALSE) {
            /* Park the sprite at x = -256, y = 240: off screen at any size. */
            u8 *low;
            u8 *high;
            u8 quarter;

            low = &oamMemory[(u16)slot << 2];
            low[0] = 0u;
            low[1] = 240u;
            high = &oamMemory[512u + (slot >> 2)];
            quarter = (u8)(slot & 3u);
            *high = (u8)((*high & loom_pvs_runtime_oam_high_keep[quarter]) |
                         loom_pvs_runtime_oam_high_x[quarter]);
        }
    }
    entry = entries;
    previous = loom_pvs_runtime_oam_previous;
    for (remaining = entry_count; remaining != 0u;
         --remaining, ++entry, ++previous) {
        u8 *low;
        u8 *high;
        u8 quarter;
        u8 slot;
        u8 bits;

        slot = entry->slot;
        loom_pvs_runtime_oam_mark[slot] = LOOM_FALSE;
        *previous = slot;
        /* Write the OAM shadow directly: pvsneslib's oamSet and oamSetEx
         * cost several scanlines per sprite. */
        low = &oamMemory[(u16)slot << 2];
        low[0] = (u8)entry->x;
        low[1] = (u8)entry->y;
        low[2] = (u8)entry->tile_index;
        bits = (u8)((entry->priority << 4) | (entry->palette << 1) |
                    (u8)((entry->tile_index >> 8) & 1u));
        if ((entry->flags & LOOM_OAM_FLAG_FLIP_Y) != 0u) {
            bits |= 0x80u;
        }
        if ((entry->flags & LOOM_OAM_FLAG_FLIP_X) != 0u) {
            bits |= 0x40u;
        }
        low[3] = bits;
        high = &oamMemory[512u + (slot >> 2)];
        quarter = (u8)(slot & 3u);
        bits = (u8)(*high & loom_pvs_runtime_oam_high_keep[quarter]);
        if (entry->x < 0) {
            bits |= loom_pvs_runtime_oam_high_x[quarter];
        }
        if (entry->size == LOOM_OAM_SIZE_LARGE) {
            bits |= loom_pvs_runtime_oam_high_large[quarter];
        }
        *high = bits;
    }
    loom_pvs_runtime_oam_previous_count = entry_count;
    return LOOM_STATUS_OK;
}
#endif

LoomStatus loom_port_init(void)
{
    loom_u8 pad;
    LoomStatus status;

    /* loom_runtime_initialize is the one-time guard. PVSnesLib does not clear
     * every same-named .bss section emitted across C translation units. */
#if defined(__65816__) && defined(__TINYC__)
    tcc__r5h = 0u;
#endif
    loom_pvs_frame_transaction_initialize();
    loom_pvs_runtime_index_chunks();
#if defined(LOOM_BUILD_DEBUG)
    /* WRIO bit 7 enables the H/V counter latch used by the tick witness. */
    *(volatile u8 *)0x4201u = 0x80u;
    loom_pvs_debug_tick_open = LOOM_FALSE;
    loom_pvs_debug_tick_cost_max_lines = 0u;
    loom_pvs_debug_tick_lag_total = 0u;
#endif
    oamClear(0u, 0u);
#if defined(__65816__)
    loom_pvs_oam_init();
#else
    loom_pvs_runtime_oam_previous_count = 0u;
    for (pad = 0u; pad < (loom_u8)(LOOM_OAM_SLOT_MAX + 1u) && pad != 0xffu;
         ++pad) {
        loom_pvs_runtime_oam_mark[pad] = LOOM_FALSE;
        if (pad == LOOM_OAM_SLOT_MAX) {
            break;
        }
    }
#endif
    loom_pvs_runtime_did_present = LOOM_FALSE;
    loom_pvs_runtime_has_presentation = LOOM_FALSE;
    loom_pvs_runtime_has_boundary = LOOM_FALSE;
    loom_pvs_runtime_last_presentation = LOOM_PRESENTATION_NONE;
    loom_pvs_runtime_missed_commit_count = 0u;
    loom_pvs_runtime_frame_id = 0u;
    loom_pvs_runtime_presented_commit_id = LOOM_COMMIT_NONE;
    for (pad = 0u; pad < LOOM_INPUT_PAD_CAPACITY; ++pad) {
        loom_pvs_runtime_input_held[pad] = 0u;
        loom_pvs_runtime_input_pressed[pad] = 0u;
        loom_pvs_runtime_input_released[pad] = 0u;
    }
    REG_HDMAEN = 0u;
    status = loom_pvs_runtime_audio_initialize();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    bgSetGfxPtr(0u, LOOM_PVS_MODE1_BG1_GFX_WORD_ADDRESS);
    bgSetMapPtr(0u, LOOM_PVS_MODE1_BG1_MAP_WORD_ADDRESS, SC_64x32);
    bgSetGfxPtr(1u, LOOM_PVS_MODE1_BG2_GFX_WORD_ADDRESS);
    bgSetMapPtr(1u, LOOM_PVS_MODE1_BG2_MAP_WORD_ADDRESS, SC_64x64);
#if LOOM_GENERATED_UI_BACKGROUND == 3
    bgSetGfxPtr(2u, LOOM_PVS_MODE1_BG3_GFX_WORD_ADDRESS);
    bgSetMapPtr(2u, LOOM_PVS_MODE1_BG3_MAP_WORD_ADDRESS, SC_64x64);
#elif defined(LOOM_GENERATED_BG3_GAMEPLAY) && LOOM_GENERATED_BG3_GAMEPLAY
    bgSetGfxPtr(2u, LOOM_PVS_MODE1_BG3_GAMEPLAY_GFX_WORD_ADDRESS);
    bgSetMapPtr(2u, LOOM_PVS_MODE1_BG3_GAMEPLAY_MAP_WORD_ADDRESS, SC_64x32);
#endif
    loom_pvs_runtime_initialized = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_frame_wait(LoomFrameBoundary *frame,
                                LoomInputSnapshot *input)
{
    loom_u8 pad;

    if (loom_pvs_runtime_initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (frame == (LoomFrameBoundary *)0 ||
        input == (LoomInputSnapshot *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_pvs_runtime_advance_physical_frame();
    frame->frame_id = loom_pvs_runtime_frame_id;
    frame->presented_commit_id =
        loom_pvs_runtime_has_presentation != LOOM_FALSE
            ? loom_pvs_runtime_presented_commit_id
            : LOOM_COMMIT_NONE;
    frame->missed_commit_count = loom_pvs_runtime_missed_commit_count;
    frame->presentation = loom_pvs_runtime_last_presentation;
    frame->deferred_dma_jobs = 0u;
    input->frame_id = frame->frame_id;
    input->pad_count = LOOM_INPUT_PAD_CAPACITY;
    input->reserved = 0u;
    {
        LoomPadFrame *pads;

        /* A pointer walk: every pads[pad] subscript is a multiply helper
         * call on 816-tcc, ten of them a tick. */
        pads = input->pads;
        for (pad = 0u; pad < LOOM_INPUT_PAD_CAPACITY; ++pad, ++pads) {
            pads->held = loom_pvs_runtime_input_held[pad];
            pads->pressed = loom_pvs_runtime_input_pressed[pad];
            pads->released = loom_pvs_runtime_input_released[pad];
            loom_pvs_runtime_input_pressed[pad] = 0u;
            loom_pvs_runtime_input_released[pad] = 0u;
        }
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_asset_read(const LoomAssetSpan *source,
                                loom_u8 *destination,
                                loom_u16 destination_capacity)
{
    loom_u16 asset_bytes;

    if (source == (const LoomAssetSpan *)0 ||
        destination == (loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (source->handle == LOOM_INVALID_HANDLE) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (loom_pvs_runtime_asset_handle_exists(source->handle) ==
        LOOM_FALSE) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    asset_bytes = loom_pvs_runtime_asset_bytes(source->handle);
    if (source->length > destination_capacity) {
        return LOOM_STATUS_CAPACITY;
    }
    if (source->offset > asset_bytes ||
        source->length > (loom_u16)(asset_bytes - source->offset) ||
        (source->length != 0u &&
         loom_pvs_runtime_asset_span_valid(source->handle,
                                           source->offset,
                                           source->length) == LOOM_FALSE)) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    loom_pvs_runtime_copy_asset(source->handle,
                                source->offset,
                                destination,
                                source->length);
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_wram_read(const LoomWramSpan *source,
                               loom_u8 *destination,
                               loom_u16 destination_capacity)
{
    (void)destination_capacity;
    if (source == (const LoomWramSpan *)0 ||
        destination == (loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    return LOOM_STATUS_UNSUPPORTED;
}

LoomStatus loom_port_wram_write(const LoomWramSpan *destination,
                                const loom_u8 *source,
                                loom_u16 source_length)
{
    (void)source_length;
    if (destination == (const LoomWramSpan *)0 ||
        source == (const loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    return LOOM_STATUS_UNSUPPORTED;
}

/* LoROM SRAM sits in bank $70; pvsneslib's console helpers copy through
 * it with a 16-bit offset. The generated header declares the size, and the
 * save module keeps every span inside it. */
LoomStatus loom_port_sram_read(loom_u16 offset,
                               loom_u8 *destination,
                               loom_u16 length)
{
    if (destination == (loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_generated_save_sram_bytes == 0u) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    if (offset > loom_generated_save_sram_bytes || length > (loom_u16)(loom_generated_save_sram_bytes - offset)) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    consoleLoadSramWithOffset(destination, length, offset);
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_sram_write(loom_u16 offset,
                                const loom_u8 *source,
                                loom_u16 length)
{
    if (source == (const loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_generated_save_sram_bytes == 0u) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    if (offset > loom_generated_save_sram_bytes || length > (loom_u16)(loom_generated_save_sram_bytes - offset)) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    consoleCopySramWithOffset((u8 *)source, length, offset);
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_audio_enqueue(const LoomAudioCueCommand *command)
{
    const LoomPvsGeneratedAudioCue *cue;
    loom_u8 index;

    if (loom_pvs_runtime_initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (command == (const LoomAudioCueCommand *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_pvs_runtime_audio_enabled == LOOM_FALSE ||
        command->command > LOOM_AUDIO_COMMAND_STOP ||
        command->bus > LOOM_AUDIO_BUS_EFFECTS) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    if (command->command == LOOM_AUDIO_COMMAND_PLAY) {
        cue = loom_pvs_runtime_audio_cue(command->cue);
        if (cue == (const LoomPvsGeneratedAudioCue *)0) {
            return LOOM_STATUS_INVALID_HANDLE;
        }
        if ((command->bus == LOOM_AUDIO_BUS_MUSIC &&
             (cue->kind != LOOM_AUDIO_CUE_KIND_MUSIC ||
              command->pan != LOOM_AUDIO_PAN_CENTER)) ||
            (command->bus == LOOM_AUDIO_BUS_EFFECTS &&
             (cue->kind != LOOM_AUDIO_CUE_KIND_EFFECT ||
              command->pan == (loom_s8)-128))) {
            return LOOM_STATUS_UNSUPPORTED;
        }
    } else if (command->bus != LOOM_AUDIO_BUS_MUSIC ||
               command->cue != LOOM_INVALID_HANDLE ||
               command->volume != LOOM_AUDIO_VOLUME_SILENT ||
               command->pan != LOOM_AUDIO_PAN_CENTER) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    if (loom_pvs_runtime_audio_count >= LOOM_PVS_AUDIO_QUEUE_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
    for (index = 0u; index < loom_pvs_runtime_audio_count; ++index) {
        if (loom_pvs_runtime_audio_queue[index].serial == command->serial) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    loom_pvs_runtime_audio_queue[loom_pvs_runtime_audio_count] = *command;
    ++loom_pvs_runtime_audio_count;
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_audio_process(void)
{
    loom_u8 index;

    if (loom_pvs_runtime_initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_pvs_runtime_audio_enabled == LOOM_FALSE) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    for (index = 0u; index < loom_pvs_runtime_audio_count; ++index) {
        LoomAudioCueCommand *command;
        const LoomPvsGeneratedAudioCue *cue;

        command = &loom_pvs_runtime_audio_queue[index];
        if (command->command == LOOM_AUDIO_COMMAND_STOP) {
            spcStop();
            loom_pvs_runtime_music_playing = LOOM_FALSE;
            continue;
        }
        cue = loom_pvs_runtime_audio_cue(command->cue);
        if (cue == (const LoomPvsGeneratedAudioCue *)0) {
            return LOOM_STATUS_INVALID_HANDLE;
        }
        if (cue->kind == LOOM_AUDIO_CUE_KIND_MUSIC) {
            if (loom_pvs_runtime_music_loaded == LOOM_FALSE ||
                loom_pvs_runtime_loaded_music != cue->handle) {
                spcLoad((u16)cue->backend_slot);
                loom_pvs_runtime_music_loaded = LOOM_TRUE;
                loom_pvs_runtime_loaded_music = cue->handle;
                loom_pvs_runtime_music_playing = LOOM_FALSE;
            }
            spcSetModuleVolume((u8)command->volume);
            if (loom_pvs_runtime_music_playing == LOOM_FALSE) {
                spcPlay(0u);
                loom_pvs_runtime_music_playing = LOOM_TRUE;
            }
        } else {
            loom_u8 volume;
            loom_u8 pan;
            loom_s16 shifted_pan;
            loom_u8 *payload;

            volume = (loom_u8)((command->volume + 16u) / 17u);
            shifted_pan = (loom_s16)command->pan + 127;
            pan = (loom_u8)((((loom_u16)shifted_pan * 15u) + 127u) /
                            254u);
            if (loom_pvs_runtime_audio_payload(cue, &payload) == LOOM_FALSE) {
                return LOOM_STATUS_INVALID_HANDLE;
            }
            spcSetSoundDataEntry(volume,
                                 pan,
                                 LOOM_PVS_AUDIO_EFFECT_PITCH,
                                 (u16)cue->byte_length,
                                 (u8 *)payload,
                                 &loom_pvs_runtime_effect_descriptor);
            spcPlaySoundV((u8)cue->backend_slot, (u16)volume);
        }
    }
    spcProcess();
    loom_pvs_runtime_audio_count = 0u;
    return LOOM_STATUS_OK;
}
