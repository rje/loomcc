#include <snes.h>

#include <loom-conformance/adapter.h>
#include <loom/port.h>

#ifndef LOOM_PVS_CONFORMANCE_AUDIO
#define LOOM_PVS_CONFORMANCE_AUDIO 0
#endif

#define LOOM_PVS_OAM_CAPACITY 33u
#define LOOM_PVS_DMA_CAPACITY 3u
#define LOOM_PVS_AUDIO_CAPACITY 3u
#define LOOM_PVS_WRAM_BYTES 64u
#define LOOM_PVS_PENDING_EVENT_CAPACITY 40u
#define LOOM_PVS_RASTER_HDMA_CHANNEL HDMA_CHANNEL6

extern loom_u8 loom_pvs_asset_bank_crossing_first;
extern loom_u8 loom_pvs_asset_bank_crossing_second;
extern loom_u8 loom_pvs_asset_oam_palette;
extern loom_u8 loom_pvs_asset_oam_tiles;
extern loom_u8 loom_pvs_audio_tone;
extern loom_u8 loom_pvs_wram_high;

typedef union LoomPvsPointerParts {
    loom_u8 *pointer;
    struct {
        loom_u16 address;
        loom_u16 bank;
    } parts;
} LoomPvsPointerParts;

static loom_u8 loom_pvs_initialized;
static loom_u8 loom_pvs_frame_open;
static volatile loom_u8 loom_pvs_frame_ready;
static volatile loom_u8 loom_pvs_did_present;
static loom_u8 loom_pvs_has_presentation;
static loom_u8 loom_pvs_has_boundary;
static loom_u8 loom_pvs_last_presentation;
static loom_u8 loom_pvs_oam_count;
static loom_u8 loom_pvs_dma_count;
static loom_u8 loom_pvs_audio_count;
static loom_u8 loom_pvs_audio_process_count;
static loom_u8 loom_pvs_pending_event_count;
static loom_u16 loom_pvs_missed_commit_count;
static LoomFrameId loom_pvs_frame_id;
volatile LoomFrameId loom_conformance_next_input_frame;
static LoomCommitId loom_pvs_presented_commit_id;
static LoomFrameCommit loom_pvs_commit;
static LoomRasterBinding loom_pvs_raster_binding;
static LoomOamEntry loom_pvs_oam[LOOM_PVS_OAM_CAPACITY];
static LoomDmaJob loom_pvs_dma[LOOM_PVS_DMA_CAPACITY];
static loom_u8 loom_pvs_dma_bounce[32];
static LoomAudioCueCommand loom_pvs_audio[LOOM_PVS_AUDIO_CAPACITY];
static LoomConformanceAdapterEvent
    loom_pvs_pending_events[LOOM_PVS_PENDING_EVENT_CAPACITY];
loom_u8 loom_pvs_wram_low[LOOM_PVS_WRAM_BYTES];
static loom_u16 loom_pvs_input_held[LOOM_INPUT_PAD_CAPACITY];
static loom_u16 loom_pvs_input_pressed[LOOM_INPUT_PAD_CAPACITY];
static loom_u16 loom_pvs_input_released[LOOM_INPUT_PAD_CAPACITY];
static loom_u8 loom_pvs_raster_table[7];
static brrsamples loom_pvs_tone_descriptor;

static void loom_pvs_sample_inputs(void);

static void loom_pvs_event(loom_u8 kind,
                           loom_u8 flags,
                           loom_u16 value_a,
                           loom_u16 value_b,
                           loom_u16 value_c)
{
    LoomConformanceAdapterEvent event;

    event.value_a = value_a;
    event.value_b = value_b;
    event.value_c = value_c;
    event.kind = kind;
    event.flags = flags;
    loom_conformance_record_adapter_event(&event);
}

static void loom_pvs_pending_event(loom_u8 kind,
                                   loom_u8 flags,
                                   loom_u16 value_a,
                                   loom_u16 value_b,
                                   loom_u16 value_c)
{
    LoomConformanceAdapterEvent *event;

    if (loom_pvs_pending_event_count >=
        LOOM_PVS_PENDING_EVENT_CAPACITY) {
        return;
    }
    event = &loom_pvs_pending_events[loom_pvs_pending_event_count];
    event->value_a = value_a;
    event->value_b = value_b;
    event->value_c = value_c;
    event->kind = kind;
    event->flags = flags;
    ++loom_pvs_pending_event_count;
}

static void loom_pvs_flush_pending_events(void)
{
    loom_u8 index;

    for (index = 0u; index < loom_pvs_pending_event_count; ++index) {
        loom_conformance_record_adapter_event(
            &loom_pvs_pending_events[index]);
    }
    loom_pvs_pending_event_count = 0u;
}

static loom_u8 loom_pvs_dma_flags(const LoomDmaJob *job)
{
    loom_u8 flags;

    flags = 0u;
    if (job->source_kind == LOOM_DMA_SOURCE_WRAM_BLOCK) {
        flags |= LOOM_CONFORMANCE_DMA_FLAG_SOURCE_WRAM;
    }
    if (job->destination_kind == LOOM_DMA_DESTINATION_CGRAM) {
        flags |= LOOM_CONFORMANCE_DMA_FLAG_DESTINATION_CGRAM;
    }
    if (job->policy == LOOM_DMA_DEFERRABLE) {
        flags |= LOOM_CONFORMANCE_DMA_FLAG_DEFERRABLE;
    }
    return flags;
}

static loom_u16 loom_pvs_asset_bytes(loom_u16 handle)
{
    if (handle == LOOM_CONFORMANCE_ASSET_BANK_CROSSING) {
        return 32u;
    }
    if (handle == LOOM_CONFORMANCE_ASSET_OAM_TILES) {
        return 64u;
    }
    if (handle == LOOM_CONFORMANCE_ASSET_OAM_PALETTE) {
        return 32u;
    }
    return 0u;
}

static loom_u8 *loom_pvs_asset_chunk(loom_u16 handle,
                                     loom_u16 offset,
                                     loom_u16 *available)
{
    if (handle == LOOM_CONFORMANCE_ASSET_BANK_CROSSING) {
        if (offset < 16u) {
            *available = (loom_u16)(16u - offset);
            return (&loom_pvs_asset_bank_crossing_first) + offset;
        }
        *available = (loom_u16)(32u - offset);
        return (&loom_pvs_asset_bank_crossing_second) +
               (loom_u16)(offset - 16u);
    }
    if (handle == LOOM_CONFORMANCE_ASSET_OAM_TILES) {
        *available = (loom_u16)(64u - offset);
        return (&loom_pvs_asset_oam_tiles) + offset;
    }
    if (handle == LOOM_CONFORMANCE_ASSET_OAM_PALETTE) {
        *available = (loom_u16)(32u - offset);
        return (&loom_pvs_asset_oam_palette) + offset;
    }
    *available = 0u;
    return (loom_u8 *)0;
}

static loom_u8 *loom_pvs_wram_block(loom_u16 handle)
{
    if (handle == LOOM_CONFORMANCE_WRAM_LOW) {
        return loom_pvs_wram_low;
    }
    if (handle == LOOM_CONFORMANCE_WRAM_HIGH) {
        return &loom_pvs_wram_high;
    }
    return (loom_u8 *)0;
}

static loom_u8 loom_pvs_source_valid(const LoomDmaJob *job)
{
    loom_u16 bytes;

    if (job->source_kind == LOOM_DMA_SOURCE_ROM_ASSET) {
        bytes = loom_pvs_asset_bytes(job->source_handle);
        if (bytes == 0u || job->source_offset > bytes ||
            job->byte_count >
                (loom_u16)(bytes - job->source_offset)) {
            return LOOM_FALSE;
        }
        return LOOM_TRUE;
    }
    if (job->source_kind == LOOM_DMA_SOURCE_WRAM_BLOCK) {
        if (loom_pvs_wram_block(job->source_handle) == (loom_u8 *)0 ||
            job->source_offset > LOOM_PVS_WRAM_BYTES ||
            job->byte_count > (loom_u16)(LOOM_PVS_WRAM_BYTES -
                                         job->source_offset)) {
            return LOOM_FALSE;
        }
        return LOOM_TRUE;
    }
    return LOOM_FALSE;
}

static void loom_pvs_dma_copy(const LoomDmaJob *job,
                              loom_u8 *source,
                              loom_u16 destination_offset,
                              loom_u16 byte_count)
{
    if (job->destination_kind == LOOM_DMA_DESTINATION_VRAM) {
        dmaCopyVram((u8 *)source,
                    (u16)(destination_offset >> 1),
                    (u16)byte_count);
    } else {
        dmaCopyCGram((u8 *)source,
                     (u16)(destination_offset >> 1),
                     (u16)byte_count);
    }
}

static void loom_pvs_execute_dma(const LoomDmaJob *job)
{
    loom_u16 remaining;
    loom_u16 source_offset;
    loom_u16 destination_offset;

    /* A direct split after an odd number of bytes would restart the VRAM or
       CGRAM port on its low-byte latch. Make the logical cross-bank span
       contiguous in private WRAM, then issue one correctly aligned DMA. */
    if (job->source_kind == LOOM_DMA_SOURCE_ROM_ASSET &&
        job->source_handle == LOOM_CONFORMANCE_ASSET_BANK_CROSSING &&
        job->source_offset < 16u &&
        (loom_u16)(job->source_offset + job->byte_count) > 16u) {
        loom_u16 copied;

        copied = 0u;
        source_offset = job->source_offset;
        while (copied < job->byte_count) {
            loom_u8 *source;
            loom_u16 available;
            loom_u16 chunk;
            loom_u16 index;

            source = loom_pvs_asset_chunk(job->source_handle,
                                          source_offset,
                                          &available);
            chunk = (loom_u16)(job->byte_count - copied);
            if (chunk > available) {
                chunk = available;
            }
            for (index = 0u; index < chunk; ++index) {
                loom_pvs_dma_bounce[(loom_u16)(copied + index)] =
                    source[index];
            }
            copied = (loom_u16)(copied + chunk);
            source_offset = (loom_u16)(source_offset + chunk);
        }
        loom_pvs_dma_copy(job,
                          loom_pvs_dma_bounce,
                          job->destination_offset,
                          job->byte_count);
        return;
    }

    remaining = job->byte_count;
    source_offset = job->source_offset;
    destination_offset = job->destination_offset;
    while (remaining != 0u) {
        loom_u8 *source;
        loom_u16 available;
        loom_u16 chunk;

        if (job->source_kind == LOOM_DMA_SOURCE_ROM_ASSET) {
            source = loom_pvs_asset_chunk(job->source_handle,
                                          source_offset,
                                          &available);
        } else {
            source = loom_pvs_wram_block(job->source_handle) +
                     source_offset;
            available = remaining;
        }
        chunk = remaining < available ? remaining : available;
        loom_pvs_dma_copy(job, source, destination_offset, chunk);
        remaining = (loom_u16)(remaining - chunk);
        source_offset = (loom_u16)(source_offset + chunk);
        destination_offset = (loom_u16)(destination_offset + chunk);
    }
}

static void loom_pvs_apply_display(const LoomDisplayState *display)
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
    REG_BG1HOFS = (u8)(((loom_u16)display->bg_scroll_x[0] >> 8) & 0x00ffu);
    REG_BG1VOFS = (u8)((loom_u16)display->bg_scroll_y[0] & 0x00ffu);
    REG_BG1VOFS = (u8)(((loom_u16)display->bg_scroll_y[0] >> 8) & 0x00ffu);
    REG_BG2HOFS = (u8)((loom_u16)display->bg_scroll_x[1] & 0x00ffu);
    REG_BG2HOFS = (u8)(((loom_u16)display->bg_scroll_x[1] >> 8) & 0x00ffu);
    REG_BG2VOFS = (u8)((loom_u16)display->bg_scroll_y[1] & 0x00ffu);
    REG_BG2VOFS = (u8)(((loom_u16)display->bg_scroll_y[1] >> 8) & 0x00ffu);
    REG_BG3HOFS = (u8)((loom_u16)display->bg_scroll_x[2] & 0x00ffu);
    REG_BG3HOFS = (u8)(((loom_u16)display->bg_scroll_x[2] >> 8) & 0x00ffu);
    REG_BG3VOFS = (u8)((loom_u16)display->bg_scroll_y[2] & 0x00ffu);
    REG_BG3VOFS = (u8)(((loom_u16)display->bg_scroll_y[2] >> 8) & 0x00ffu);
    REG_BG4HOFS = (u8)((loom_u16)display->bg_scroll_x[3] & 0x00ffu);
    REG_BG4HOFS = (u8)(((loom_u16)display->bg_scroll_x[3] >> 8) & 0x00ffu);
    REG_BG4VOFS = (u8)((loom_u16)display->bg_scroll_y[3] & 0x00ffu);
    REG_BG4VOFS = (u8)(((loom_u16)display->bg_scroll_y[3] >> 8) & 0x00ffu);

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

static loom_u8 loom_pvs_fixed_color_byte(loom_u8 color)
{
    if (color == 0u) {
        return 0x3fu;
    }
    if (color == 1u) {
        return 0x5fu;
    }
    return 0x9fu;
}

static void loom_pvs_apply_raster(const LoomRasterBinding *binding)
{
    loom_u8 first_color;
    loom_u8 second_color;
    LoomPvsPointerParts pointer;

    REG_HDMAEN = 0u;
    if (binding->program == LOOM_RASTER_PROGRAM_NONE) {
        return;
    }
    if (binding->program == LOOM_CONFORMANCE_RASTER_STATIC) {
        first_color = 0u;
        second_color = 2u;
    } else if (binding->state == LOOM_CONFORMANCE_RASTER_STATE_A) {
        first_color = 1u;
        second_color = 2u;
    } else {
        first_color = 2u;
        second_color = 0u;
    }

    /* Mode 2 writes two bytes to COLDATA: clear RGB, then set one channel. */
    loom_pvs_raster_table[0] = 112u;
    loom_pvs_raster_table[1] = 0xe0u;
    loom_pvs_raster_table[2] = loom_pvs_fixed_color_byte(first_color);
    loom_pvs_raster_table[3] = 112u;
    loom_pvs_raster_table[4] = 0xe0u;
    loom_pvs_raster_table[5] = loom_pvs_fixed_color_byte(second_color);
    loom_pvs_raster_table[6] = 0u;

    pointer.pointer = loom_pvs_raster_table;
    REG_DMAP6 = 2u;
    REG_BBAD6 = 0x32u;
    REG_A1T6LH = (u16)pointer.parts.address;
    REG_A1B6 = (u8)(pointer.parts.bank & 0x00ffu);
    REG_HDMAEN = LOOM_PVS_RASTER_HDMA_CHANNEL;
}

static void loom_pvs_prepare_oam(void)
{
    loom_u8 index;

    oamClear(0u, 0u);
    for (index = 0u; index < loom_pvs_oam_count; ++index) {
        LoomOamEntry *entry;
        u16 id;

        entry = &loom_pvs_oam[index];
        id = (u16)((u16)entry->slot * 4u);
        oamSet(id,
               (u16)entry->x,
               (u16)entry->y,
               (u8)entry->priority,
               (u8)((entry->flags & LOOM_OAM_FLAG_FLIP_X) != 0u),
               (u8)((entry->flags & LOOM_OAM_FLAG_FLIP_Y) != 0u),
               (u16)entry->tile_index,
               (u8)entry->palette);
        oamSetEx(id,
                 entry->size == LOOM_OAM_SIZE_LARGE ? OBJ_LARGE
                                                     : OBJ_SMALL,
                 OBJ_SHOW);
    }
}

static void loom_pvs_vblank(void)
{
    loom_u8 index;

    loom_pvs_sample_inputs();
    if (loom_pvs_frame_ready == LOOM_FALSE) {
        return;
    }

    for (index = 0u; index < loom_pvs_dma_count; ++index) {
        loom_pvs_execute_dma(&loom_pvs_dma[index]);
    }
    loom_pvs_apply_display(&loom_pvs_commit.display);
    /* OAM is whole-frame state: even an empty commit must hide sprites left
       visible by the preceding presentation. */
    oamUpdate();
    if (loom_pvs_commit.raster.program !=
            loom_pvs_raster_binding.program ||
        loom_pvs_commit.raster.state != loom_pvs_raster_binding.state) {
        loom_pvs_apply_raster(&loom_pvs_commit.raster);
        loom_pvs_raster_binding = loom_pvs_commit.raster;
    }
    loom_pvs_presented_commit_id = loom_pvs_commit.commit_id;
    loom_pvs_has_presentation = LOOM_TRUE;
    loom_pvs_frame_ready = LOOM_FALSE;
    loom_pvs_did_present = LOOM_TRUE;
}

static void loom_pvs_sample_inputs(void)
{
    loom_u8 pad;

    while ((REG_HVBJOY & 0x01u) != 0u) {
    }
    for (pad = 0u; pad < LOOM_INPUT_PAD_CAPACITY; ++pad) {
        loom_u16 current;
        loom_u16 previous;

        current = (loom_u16)REG_JOYxLH(pad);
        previous = loom_pvs_input_held[pad];
        loom_pvs_input_pressed[pad] |=
            (loom_u16)(current & (loom_u16)(~previous));
        loom_pvs_input_released[pad] |=
            (loom_u16)(previous & (loom_u16)(~current));
        loom_pvs_input_held[pad] = current;
    }
}

static void loom_pvs_advance_physical_frame(void)
{
    WaitForVBlank();
    if (loom_pvs_did_present != LOOM_FALSE) {
        loom_pvs_flush_pending_events();
    }

    if (loom_pvs_has_boundary == LOOM_FALSE) {
        loom_pvs_frame_id = 0u;
        loom_pvs_last_presentation = LOOM_PRESENTATION_NONE;
        loom_pvs_has_boundary = LOOM_TRUE;
    } else {
        ++loom_pvs_frame_id;
        if (loom_pvs_did_present != LOOM_FALSE) {
            loom_pvs_last_presentation = LOOM_PRESENTATION_NEW_COMMIT;
        } else if (loom_pvs_has_presentation != LOOM_FALSE) {
            if (loom_pvs_missed_commit_count != LOOM_MISSED_COMMIT_MAX) {
                ++loom_pvs_missed_commit_count;
            }
            loom_pvs_last_presentation = LOOM_PRESENTATION_REPEATED;
        } else {
            loom_pvs_last_presentation = LOOM_PRESENTATION_NONE;
        }
    }
    if (loom_conformance_next_input_frame != 0xffffu) {
        ++loom_conformance_next_input_frame;
    }
    loom_pvs_did_present = LOOM_FALSE;
}

static void loom_pvs_sort_oam(void)
{
    loom_u8 index;

    for (index = 1u; index < loom_pvs_oam_count; ++index) {
        LoomOamEntry entry;
        loom_u8 position;

        entry = loom_pvs_oam[index];
        position = index;
        while (position != 0u &&
               loom_pvs_oam[(loom_u8)(position - 1u)].slot > entry.slot) {
            loom_pvs_oam[position] =
                loom_pvs_oam[(loom_u8)(position - 1u)];
            --position;
        }
        loom_pvs_oam[position] = entry;
    }
}

static void loom_pvs_prepare_frame_events(void)
{
    loom_u8 index;

    loom_pvs_pending_event_count = 0u;
    for (index = 0u; index < loom_pvs_dma_count; ++index) {
        loom_pvs_pending_event(LOOM_CONFORMANCE_EVENT_DMA_EXECUTE,
                               loom_pvs_dma_flags(&loom_pvs_dma[index]),
                               loom_pvs_dma[index].job_id,
                               loom_pvs_dma[index].destination_offset,
                               loom_pvs_dma[index].byte_count);
    }
    if (loom_pvs_oam_count != 0u) {
        loom_u8 first_hidden;
        loom_u8 hidden_count;
        loom_u8 scanline_count;
        loom_u16 slot;

        first_hidden = 0xffu;
        hidden_count = 0u;
        scanline_count = 0u;
        index = 0u;
        for (slot = 0u; slot <= (loom_u16)LOOM_OAM_SLOT_MAX; ++slot) {
            if (index < loom_pvs_oam_count &&
                loom_pvs_oam[index].slot == (loom_u8)slot) {
                LoomOamEntry *entry;

                entry = &loom_pvs_oam[index];
                loom_pvs_pending_event(
                    LOOM_CONFORMANCE_EVENT_OAM_PACKED,
                    entry->flags,
                    (loom_u16)entry->slot,
                    entry->tile_index,
                    (loom_u16)entry->size);
                if (entry->y == 80) {
                    ++scanline_count;
                }
                ++index;
            } else {
                if (first_hidden == 0xffu) {
                    first_hidden = (loom_u8)slot;
                }
                ++hidden_count;
            }
        }
        loom_pvs_pending_event(LOOM_CONFORMANCE_EVENT_OAM_HIDDEN,
                               LOOM_CONFORMANCE_EVENT_FLAG_NONE,
                               (loom_u16)hidden_count,
                               (loom_u16)first_hidden,
                               0u);
        if (scanline_count > 32u) {
            loom_pvs_pending_event(LOOM_CONFORMANCE_EVENT_OAM_SCANLINE,
                                   LOOM_CONFORMANCE_EVENT_FLAG_NONE,
                                   (loom_u16)scanline_count,
                                   80u,
                                   32u);
        }
    }
    if (loom_pvs_commit.raster.program !=
            loom_pvs_raster_binding.program ||
        loom_pvs_commit.raster.state != loom_pvs_raster_binding.state) {
        loom_pvs_pending_event(LOOM_CONFORMANCE_EVENT_RASTER_BIND,
                               LOOM_CONFORMANCE_EVENT_FLAG_NONE,
                               loom_pvs_commit.raster.program,
                               loom_pvs_commit.raster.state,
                               (loom_u16)loom_pvs_dma_count);
    }
}

LoomStatus loom_port_init(void)
{
    loom_u8 index;

    loom_pvs_initialized = LOOM_TRUE;
    loom_pvs_frame_open = LOOM_FALSE;
    loom_pvs_frame_ready = LOOM_FALSE;
    loom_pvs_did_present = LOOM_FALSE;
    loom_pvs_has_presentation = LOOM_FALSE;
    loom_pvs_has_boundary = LOOM_FALSE;
    loom_pvs_last_presentation = LOOM_PRESENTATION_NONE;
    loom_pvs_oam_count = 0u;
    loom_pvs_dma_count = 0u;
    loom_pvs_audio_count = 0u;
    loom_pvs_audio_process_count = 0u;
    loom_pvs_pending_event_count = 0u;
    loom_pvs_missed_commit_count = 0u;
    loom_pvs_frame_id = 0u;
    loom_conformance_next_input_frame = 0u;
    loom_pvs_presented_commit_id = LOOM_COMMIT_NONE;
    loom_pvs_raster_binding.program = LOOM_RASTER_PROGRAM_NONE;
    loom_pvs_raster_binding.state = LOOM_RASTER_STATE_NONE;
    for (index = 0u; index < LOOM_INPUT_PAD_CAPACITY; ++index) {
        loom_pvs_input_held[index] = 0u;
        loom_pvs_input_pressed[index] = 0u;
        loom_pvs_input_released[index] = 0u;
    }
    for (index = 0u; index < LOOM_PVS_WRAM_BYTES; ++index) {
        loom_pvs_wram_low[index] = 0u;
        (&loom_pvs_wram_high)[index] = 0u;
    }
    REG_HDMAEN = 0u;

#if LOOM_PVS_CONFORMANCE_AUDIO
    spcBoot();
    spcAllocateSoundRegion(1u);
    spcSetSoundEntry(15u,
                     8u,
                     4u,
                     9u,
                     (u8 *)&loom_pvs_audio_tone,
                     &loom_pvs_tone_descriptor);
#endif
    /* spcBoot disables NMI, so install the Loom callback only after optional
       SPC initialization has completed. */
    nmiSet(loom_pvs_vblank);
    return LOOM_STATUS_OK;
}

LoomStatus loom_conformance_hold_physical_frames(loom_u8 frame_count)
{
    loom_u8 frame;

    if (loom_pvs_initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (frame_count == 0u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        loom_pvs_advance_physical_frame();
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_frame_wait(LoomFrameBoundary *frame,
                                LoomInputSnapshot *input)
{
    loom_u8 pad;

    if (loom_pvs_initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (frame == (LoomFrameBoundary *)0 ||
        input == (LoomInputSnapshot *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_pvs_advance_physical_frame();
    frame->frame_id = loom_pvs_frame_id;
    frame->presented_commit_id =
        loom_pvs_has_presentation != LOOM_FALSE
            ? loom_pvs_presented_commit_id
            : LOOM_COMMIT_NONE;
    frame->missed_commit_count = loom_pvs_missed_commit_count;
    frame->presentation = loom_pvs_last_presentation;
    frame->deferred_dma_jobs = 0u;
    input->frame_id = frame->frame_id;
    input->pad_count = LOOM_INPUT_PAD_CAPACITY;
    input->reserved = 0u;
    for (pad = 0u; pad < LOOM_INPUT_PAD_CAPACITY; ++pad) {
        input->pads[pad].held = loom_pvs_input_held[pad];
        input->pads[pad].pressed = loom_pvs_input_pressed[pad];
        input->pads[pad].released = loom_pvs_input_released[pad];
        loom_pvs_input_pressed[pad] = 0u;
        loom_pvs_input_released[pad] = 0u;
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_frame_begin(const LoomFrameCommit *commit)
{
    if (loom_pvs_initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (commit == (const LoomFrameCommit *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_pvs_frame_open != LOOM_FALSE ||
        loom_pvs_frame_ready != LOOM_FALSE) {
        return LOOM_STATUS_BUSY;
    }
    if (commit->oam_count > LOOM_PVS_OAM_CAPACITY ||
        commit->dma_count > LOOM_PVS_DMA_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
    loom_pvs_commit = *commit;
    loom_pvs_oam_count = 0u;
    loom_pvs_dma_count = 0u;
    loom_pvs_frame_open = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_oam_stage(const LoomOamEntry *entry)
{
    loom_u8 index;

    if (loom_pvs_frame_open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (entry == (const LoomOamEntry *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_pvs_oam_count >= loom_pvs_commit.oam_count) {
        return LOOM_STATUS_CAPACITY;
    }
    if (entry->slot > LOOM_OAM_SLOT_MAX ||
        entry->tile_index > LOOM_OAM_TILE_INDEX_MAX ||
        entry->palette > 7u || entry->priority > 3u ||
        entry->size > LOOM_OAM_SIZE_LARGE ||
        (entry->flags &
         (loom_u8)(~(LOOM_OAM_FLAG_FLIP_X |
                     LOOM_OAM_FLAG_FLIP_Y))) != 0u ||
        entry->reserved != 0u || entry->x < LOOM_OAM_X_MIN ||
        entry->x > LOOM_OAM_X_MAX || entry->y < LOOM_OAM_Y_MIN ||
        entry->y > LOOM_OAM_Y_MAX) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < loom_pvs_oam_count; ++index) {
        if (loom_pvs_oam[index].slot == entry->slot) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    loom_pvs_oam[loom_pvs_oam_count] = *entry;
    ++loom_pvs_oam_count;
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_dma_stage(const LoomDmaJob *job)
{
    loom_u8 index;

    if (loom_pvs_frame_open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (job == (const LoomDmaJob *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_pvs_dma_count >= loom_pvs_commit.dma_count) {
        return LOOM_STATUS_CAPACITY;
    }
    if (job->byte_count == 0u || (job->byte_count & 1u) != 0u ||
        (job->destination_offset & 1u) != 0u || job->reserved != 0u ||
        job->source_kind > LOOM_DMA_SOURCE_WRAM_BLOCK ||
        job->destination_kind > LOOM_DMA_DESTINATION_CGRAM ||
        job->policy > LOOM_DMA_DEFERRABLE ||
        loom_pvs_source_valid(job) == LOOM_FALSE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if ((job->destination_kind == LOOM_DMA_DESTINATION_VRAM &&
         job->destination_offset >
             (loom_u16)(0xffffu - (job->byte_count - 1u))) ||
        (job->destination_kind == LOOM_DMA_DESTINATION_CGRAM &&
         (job->byte_count > 512u ||
          job->destination_offset >
              (loom_u16)(511u - (job->byte_count - 1u))))) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < loom_pvs_dma_count; ++index) {
        if (loom_pvs_dma[index].job_id == job->job_id) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    loom_pvs_dma[loom_pvs_dma_count] = *job;
    ++loom_pvs_dma_count;
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_frame_commit(void)
{
    if (loom_pvs_frame_open == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_pvs_oam_count != loom_pvs_commit.oam_count ||
        loom_pvs_dma_count != loom_pvs_commit.dma_count) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_pvs_sort_oam();
    loom_pvs_prepare_frame_events();
    loom_pvs_prepare_oam();
    loom_pvs_frame_open = LOOM_FALSE;
    loom_pvs_frame_ready = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

void loom_port_frame_abort(void)
{
    loom_pvs_frame_open = LOOM_FALSE;
    loom_pvs_oam_count = 0u;
    loom_pvs_dma_count = 0u;
    loom_pvs_pending_event_count = 0u;
}

LoomStatus loom_port_asset_read(const LoomAssetSpan *source,
                                loom_u8 *destination,
                                loom_u16 destination_capacity)
{
    loom_u16 bytes;
    loom_u16 offset;
    loom_u16 remaining;

    if (source == (const LoomAssetSpan *)0 ||
        destination == (loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    bytes = loom_pvs_asset_bytes(source->handle);
    if (bytes == 0u) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (source->length > destination_capacity) {
        return LOOM_STATUS_CAPACITY;
    }
    if (source->offset > bytes ||
        source->length > (loom_u16)(bytes - source->offset)) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }

    offset = source->offset;
    remaining = source->length;
    while (remaining != 0u) {
        loom_u8 *chunk_source;
        loom_u16 available;
        loom_u16 chunk;
        loom_u16 index;

        chunk_source = loom_pvs_asset_chunk(source->handle,
                                            offset,
                                            &available);
        chunk = remaining < available ? remaining : available;
        for (index = 0u; index < chunk; ++index) {
            *destination = chunk_source[index];
            ++destination;
        }
        loom_pvs_event(LOOM_CONFORMANCE_EVENT_ASSET_CHUNK,
                       LOOM_CONFORMANCE_EVENT_FLAG_READ,
                       source->handle,
                       offset,
                       chunk);
        offset = (loom_u16)(offset + chunk);
        remaining = (loom_u16)(remaining - chunk);
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_wram_read(const LoomWramSpan *source,
                               loom_u8 *destination,
                               loom_u16 destination_capacity)
{
    loom_u8 *block;
    loom_u16 index;

    if (source == (const LoomWramSpan *)0 ||
        destination == (loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    block = loom_pvs_wram_block(source->handle);
    if (block == (loom_u8 *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (source->length > destination_capacity) {
        return LOOM_STATUS_CAPACITY;
    }
    if (source->offset > LOOM_PVS_WRAM_BYTES ||
        source->length >
            (loom_u16)(LOOM_PVS_WRAM_BYTES - source->offset)) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    for (index = 0u; index < source->length; ++index) {
        destination[index] = block[(loom_u16)(source->offset + index)];
    }
    loom_pvs_event(LOOM_CONFORMANCE_EVENT_WRAM_ACCESS,
                   LOOM_CONFORMANCE_EVENT_FLAG_READ,
                   source->handle,
                   source->offset,
                   source->length);
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_wram_write(const LoomWramSpan *destination,
                                const loom_u8 *source,
                                loom_u16 source_length)
{
    loom_u8 *block;
    loom_u16 index;

    if (destination == (const LoomWramSpan *)0 ||
        source == (const loom_u8 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    block = loom_pvs_wram_block(destination->handle);
    if (block == (loom_u8 *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (source_length != destination->length) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    if (destination->offset > LOOM_PVS_WRAM_BYTES ||
        destination->length >
            (loom_u16)(LOOM_PVS_WRAM_BYTES - destination->offset)) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    for (index = 0u; index < source_length; ++index) {
        block[(loom_u16)(destination->offset + index)] = source[index];
    }
    loom_pvs_event(LOOM_CONFORMANCE_EVENT_WRAM_ACCESS,
                   LOOM_CONFORMANCE_EVENT_FLAG_WRITE,
                   destination->handle,
                   destination->offset,
                   destination->length);
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_audio_enqueue(const LoomAudioCueCommand *command)
{
    loom_u8 index;

    if (command == (const LoomAudioCueCommand *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (command->command > LOOM_AUDIO_COMMAND_STOP ||
        command->bus > LOOM_AUDIO_BUS_EFFECTS) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (command->command == LOOM_AUDIO_COMMAND_PLAY &&
        command->cue == LOOM_INVALID_HANDLE) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (command->command == LOOM_AUDIO_COMMAND_PLAY &&
        command->cue != LOOM_CONFORMANCE_AUDIO_EFFECT &&
        command->cue != LOOM_CONFORMANCE_AUDIO_MUSIC) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (command->command == LOOM_AUDIO_COMMAND_PLAY &&
        command->bus == LOOM_AUDIO_BUS_MUSIC && command->pan != 0) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    if (command->command == LOOM_AUDIO_COMMAND_PLAY &&
        command->bus == LOOM_AUDIO_BUS_EFFECTS &&
        command->pan == (loom_s8)-128) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    if (command->command == LOOM_AUDIO_COMMAND_STOP &&
        (command->bus != LOOM_AUDIO_BUS_MUSIC ||
         command->cue != LOOM_INVALID_HANDLE || command->volume != 0u ||
         command->pan != 0)) {
        return LOOM_STATUS_UNSUPPORTED;
    }
    if (loom_pvs_audio_count >= LOOM_PVS_AUDIO_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
    for (index = 0u; index < loom_pvs_audio_count; ++index) {
        if (loom_pvs_audio[index].serial == command->serial) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    loom_pvs_audio[loom_pvs_audio_count] = *command;
    ++loom_pvs_audio_count;
    return LOOM_STATUS_OK;
}

LoomStatus loom_port_audio_process(void)
{
    loom_u8 index;

    if (loom_pvs_initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    for (index = 0u; index < loom_pvs_audio_count; ++index) {
        LoomAudioCueCommand *command;
        loom_u8 flags;

        command = &loom_pvs_audio[index];
        flags = 0u;
        if (command->bus == LOOM_AUDIO_BUS_EFFECTS) {
            flags |= LOOM_CONFORMANCE_AUDIO_FLAG_BUS_EFFECTS;
        }
        if (command->command == LOOM_AUDIO_COMMAND_STOP) {
            flags |= LOOM_CONFORMANCE_AUDIO_FLAG_STOP;
        }
        loom_pvs_event(LOOM_CONFORMANCE_EVENT_AUDIO_DISPATCH,
                       flags,
                       command->serial,
                       command->cue,
                       command->frame_id);
        loom_pvs_event(
            LOOM_CONFORMANCE_EVENT_AUDIO_PARAMETERS,
            flags,
            command->serial,
            command->frame_id,
            (loom_u16)((loom_u16)command->volume |
                       (loom_u16)((loom_u16)(loom_u8)command->pan << 8)));

        if (command->command == LOOM_AUDIO_COMMAND_STOP) {
            spcStop();
        } else {
            loom_u8 volume;
            loom_u8 pan;
            loom_s16 shifted_pan;

            volume = (loom_u8)((command->volume + 16u) / 17u);
            shifted_pan = (loom_s16)command->pan + 127;
            pan = (loom_u8)(((loom_u16)shifted_pan * 15u) / 254u);
            spcSetSoundDataEntry(volume,
                                 pan,
                                 4u,
                                 9u,
                                 (u8 *)&loom_pvs_audio_tone,
                                 &loom_pvs_tone_descriptor);
            spcPlaySoundV(0u, (u16)volume);
        }
    }
    spcProcess();
    ++loom_pvs_audio_process_count;
    loom_pvs_event(LOOM_CONFORMANCE_EVENT_AUDIO_PROCESS,
                   LOOM_CONFORMANCE_EVENT_FLAG_NONE,
                   loom_pvs_audio_process_count,
                   (loom_u16)loom_pvs_audio_count,
                   0u);
    loom_pvs_audio_count = 0u;
    return LOOM_STATUS_OK;
}
