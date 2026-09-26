#include <loom/mode1.h>
#include <loom/runtime.h>
#include <loom/ui.h>

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
/* runtime/backends/pvsneslib/src/oam.asm builds the sprites straight into
 * the OAM shadow; the C loop below is the portable rendition. */
#define LOOM_MODE1_SPRITE_BATCH 1
LoomStatus loom_pvs_oam_batch(const LoomMode1SpriteBatch *batch);
#endif

typedef struct LoomMode1State {
    loom_u16 load_segment;
    loom_u16 load_offset;
    loom_u16 pending_segment;
    loom_u16 pending_offset;
    loom_u16 next_job_id;
    LoomCommitId pending_commit_id;
    loom_s16 camera_x;
    loom_s16 camera_y;
    loom_u8 pending_load;
    loom_u8 ready;
    loom_u8 initialized;
    loom_u8 brightness;
    loom_u8 raster_enabled;
    const LoomMode1Scene *resident_scene;
    const LoomMode1Scene *scene;
    loom_s16 layer_auto_x[LOOM_MODE1_LAYER_CAPACITY];
    loom_s16 layer_auto_y[LOOM_MODE1_LAYER_CAPACITY];
    loom_s16 layer_auto_frac_x[LOOM_MODE1_LAYER_CAPACITY];
    loom_s16 layer_auto_frac_y[LOOM_MODE1_LAYER_CAPACITY];
    loom_s16 sprite_world_x[LOOM_FRAME_OAM_CAPACITY];
    loom_s16 sprite_world_y[LOOM_FRAME_OAM_CAPACITY];
    loom_u8 sprite_visible[LOOM_FRAME_OAM_CAPACITY];
    /* Each sprite's part count, mirrored out of the scene record so moving
     * a metasprite reads one byte instead of indexing a twenty-byte struct,
     * which is a multiply helper call on 816-tcc. */
    loom_u8 sprite_part_count[LOOM_FRAME_OAM_CAPACITY];
    LoomMode1SpritePose sprite_pose[LOOM_FRAME_OAM_CAPACITY];
    /* Streaming: the loaded window's origin and the last camera row seen. */
    loom_u16 stream_column;
    loom_u16 stream_row;
    loom_s16 stream_last_camera_y;
    loom_u16 stream_job_id;
    /* Sprite index per OAM slot, 0xff when the slot is unused. */
    loom_u8 slot_index[LOOM_OAM_SLOT_MAX + 1u];
    /* Tile animations and palette cycles: the frame or step shown, ticks
     * spent on it, and a bit per entry whose next frame is due but not yet
     * staged. One animation and one cycle stage per tick, round robin. */
    loom_u8 animation_frame[LOOM_MODE1_TILE_ANIMATIONS_MAX];
    loom_u16 animation_elapsed[LOOM_MODE1_TILE_ANIMATIONS_MAX];
    loom_u8 animation_due;
    loom_u8 animation_next;
    loom_u8 cycle_step[LOOM_MODE1_PALETTE_CYCLES_MAX];
    loom_u16 cycle_elapsed[LOOM_MODE1_PALETTE_CYCLES_MAX];
    loom_u8 cycle_due;
    loom_u8 cycle_next;
    /* Raster: which WRAM table the next frame reads, and the wave's phase. */
    loom_u8 raster_buffer;
    loom_u16 wave_phase;
} LoomMode1State;

static LoomMode1State loom_mode1_state;
loom_u8 loom_mode1_raster_tables[2][LOOM_MODE1_RASTER_TABLE_BYTES];
static loom_u16 loom_mode1_stream_buffer[LOOM_MODE1_STREAM_BUFFER_WORDS];

static loom_s16 loom_mode1_clamp(loom_s16 value,
                                 loom_s16 minimum,
                                 loom_s16 maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

/* The window origin limits: zero when the scene fits the window on that
 * axis, since the blank tile fills the rest of the hardware map. */
static loom_u16 loom_mode1_stream_max_column(const LoomMode1Stream *stream)
{
    return stream->width_metatiles > LOOM_MODE1_STREAM_WINDOW_COLUMNS
               ? (loom_u16)(stream->width_metatiles -
                            LOOM_MODE1_STREAM_WINDOW_COLUMNS)
               : 0u;
}

static loom_u16 loom_mode1_stream_max_row(const LoomMode1Stream *stream)
{
    return stream->height_metatiles > LOOM_MODE1_STREAM_WINDOW_ROWS
               ? (loom_u16)(stream->height_metatiles -
                            LOOM_MODE1_STREAM_WINDOW_ROWS)
               : 0u;
}

static LoomStatus loom_mode1_validate_scene(const LoomMode1Scene *scene)
{
    loom_u8 index;

    if (scene->reserved != 0u || scene->pixel_width < LOOM_MODE1_VIEW_WIDTH ||
        scene->pixel_height < LOOM_MODE1_VIEW_HEIGHT ||
        scene->camera_min_x > scene->camera_max_x ||
        scene->camera_min_y > scene->camera_max_y ||
        (scene->raster.program == LOOM_RASTER_PROGRAM_NONE &&
         scene->raster.state != LOOM_RASTER_STATE_NONE) ||
        scene->obj_size_pair > LOOM_OBJ_SIZE_32_64 ||
        scene->sprite_count > LOOM_FRAME_OAM_CAPACITY ||
        (scene->load_segment_count != 0u &&
         scene->load_segments == (const LoomMode1LoadSegment *)0) ||
        (scene->sprite_count != 0u &&
         scene->sprites == (const LoomMode1Sprite *)0) ||
        scene->layer_count > LOOM_MODE1_LAYER_CAPACITY ||
        scene->layer_reserved != 0u ||
        (scene->layer_count != 0u &&
         scene->layers == (const LoomMode1Layer *)0)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (scene->color_math_reserved != 0u ||
        scene->color_math_mode > LOOM_MODE1_COLOR_MATH_DARKEN ||
        (scene->color_math_layers & (loom_u8)(~0x07u)) != 0u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (scene->color_math_mode != LOOM_MODE1_COLOR_MATH_NORMAL &&
        scene->color_math_layers == 0u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (scene->color_math_mode == LOOM_MODE1_COLOR_MATH_DARKEN &&
        (scene->color_math_amount == 0u || scene->color_math_amount > 31u)) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (scene->raster_params != (const LoomMode1RasterParams *)0) {
        const LoomMode1RasterParams *params;

        params = scene->raster_params;
        if (params->reserved0 != 0u || params->reserved1 != 0u ||
            params->layer > 2u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (params->kind == LOOM_MODE1_RASTER_SCROLL_BANDS) {
            if (params->band_count == 0u ||
                params->band_count > LOOM_MODE1_RASTER_BANDS_MAX) {
                return LOOM_STATUS_INVALID_ARGUMENT;
            }
            for (index = 0u; index < params->band_count; ++index) {
                if (params->band_lines[index] == 0u ||
                    params->band_denominators[index] == 0u) {
                    return LOOM_STATUS_INVALID_ARGUMENT;
                }
            }
        } else if (params->kind == LOOM_MODE1_RASTER_WAVE) {
            if (params->wave_wavelength == 0u ||
                params->wave_amplitude == 0u) {
                return LOOM_STATUS_INVALID_ARGUMENT;
            }
        } else if (params->kind != LOOM_MODE1_RASTER_FIXED_COLOR &&
                   params->kind != LOOM_MODE1_RASTER_BACKDROP_GRADIENT) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (scene->raster.program == LOOM_RASTER_PROGRAM_NONE) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    } else if (scene->raster.program != LOOM_RASTER_PROGRAM_NONE) {
        /* Every program says what the runtime owes it, even nothing. */
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (scene->stream != (const LoomMode1Stream *)0) {
        const LoomMode1Stream *stream;
        loom_u16 width;
        loom_u16 height;
        loom_u16 width_limit;

        /* One check per statement, with the division in a local: 816-tcc
         * miscompiled this as a single || chain, rejecting a 64-column
         * room on the console (a 40-column one passed) while the same
         * source was correct on the host. Keep it flat. */
        stream = scene->stream;
        width = stream->width_metatiles;
        height = stream->height_metatiles;
        if (stream->reserved != 0u || width == 0u || height == 0u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (width > LOOM_MODE1_STREAM_SPAN_MAX ||
            height > LOOM_MODE1_STREAM_SPAN_MAX) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        width_limit = (loom_u16)(LOOM_MODE1_STREAM_CELLS_MAX / height);
        if (width > width_limit) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (stream->metatile_count == 0u ||
            stream->metatile_count == LOOM_MODE1_STREAM_BLANK ||
            stream->grid == (const loom_u8 *)0 ||
            stream->table == (const loom_u16 *)0) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (stream->initial_column > loom_mode1_stream_max_column(stream)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (stream->initial_row > loom_mode1_stream_max_row(stream)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        if (scene->pixel_width != (loom_u16)(width * 16u) ||
            scene->pixel_height != (loom_u16)(height * 16u)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u; index < scene->layer_count; ++index) {
        const LoomMode1Layer *layer;

        layer = &scene->layers[index];
        if (layer->background >= LOOM_MODE1_LAYER_CAPACITY ||
            layer->above_sprites > 1u ||
            layer->scroll_x_denominator == 0u ||
            layer->scroll_y_denominator == 0u ||
            layer->scroll_x_numerator > LOOM_MODE1_SCROLL_TERM_MAX ||
            layer->scroll_x_denominator > LOOM_MODE1_SCROLL_TERM_MAX ||
            layer->scroll_y_numerator > LOOM_MODE1_SCROLL_TERM_MAX ||
            layer->scroll_y_denominator > LOOM_MODE1_SCROLL_TERM_MAX) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u; index < scene->load_segment_count; ++index) {
        const LoomMode1LoadSegment *segment;

        segment = &scene->load_segments[index];
        if (segment->source_handle == LOOM_INVALID_HANDLE ||
            segment->byte_count == 0u || segment->reserved != 0u ||
            (segment->destination_kind != LOOM_DMA_DESTINATION_VRAM &&
             segment->destination_kind != LOOM_DMA_DESTINATION_CGRAM)) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    for (index = 0u; index < scene->sprite_count; ++index) {
        const LoomMode1Sprite *sprite;

        sprite = &scene->sprites[index];
        if (sprite->slot > LOOM_OAM_SLOT_MAX || sprite->palette > 7u ||
            sprite->priority > 3u || sprite->size > LOOM_OAM_SIZE_LARGE ||
            sprite->tile_index > LOOM_OAM_TILE_INDEX_MAX ||
            (sprite->flags & (loom_u8)(~0x03u)) != 0u ||
            sprite->width == 0u || sprite->height == 0u) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    return LOOM_STATUS_OK;
}

#if !defined(LOOM_TARGET_PVSNESLIB)
static LoomStatus loom_mode1_validate_pose(const LoomMode1SpritePose *pose)
{
    if (pose == (const LoomMode1SpritePose *)0 || pose->palette > 7u ||
        pose->size > LOOM_OAM_SIZE_LARGE ||
        pose->tile_index > LOOM_OAM_TILE_INDEX_MAX ||
        pose->width == 0u || pose->height == 0u ||
        (pose->flags &
         (loom_u8)(~(LOOM_OAM_FLAG_FLIP_X | LOOM_OAM_FLAG_FLIP_Y))) != 0u) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    return LOOM_STATUS_OK;
}
#endif

static loom_u8 loom_mode1_segments_equal(
    const LoomMode1LoadSegment *left,
    const LoomMode1LoadSegment *right)
{
    return (loom_u8)(left->source_handle == right->source_handle &&
                     left->source_offset == right->source_offset &&
                     left->destination_offset == right->destination_offset &&
                     left->byte_count == right->byte_count &&
                     left->destination_kind == right->destination_kind &&
                     left->reserved == right->reserved);
}

static loom_u8 loom_mode1_segment_is_resident(loom_u16 index)
{
    const LoomMode1Scene *scene;
    const LoomMode1Scene *resident;

    scene = loom_mode1_state.scene;
    resident = loom_mode1_state.resident_scene;
    if (resident == (const LoomMode1Scene *)0 ||
        index >= scene->load_segment_count ||
        index >= resident->load_segment_count) {
        return LOOM_FALSE;
    }
    /* Streaming rewrites the BG1 map after activation, so a streamed
     * scene's map segment never counts as resident. */
    if (resident->stream != (const LoomMode1Stream *)0 &&
        scene->load_segments[index].destination_kind ==
            LOOM_DMA_DESTINATION_VRAM &&
        scene->load_segments[index].destination_offset >=
            LOOM_MODE1_BG1_MAP_VRAM_BYTE &&
        scene->load_segments[index].destination_offset <
            (loom_u16)(LOOM_MODE1_BG1_MAP_VRAM_BYTE + 4096u)) {
        return LOOM_FALSE;
    }
    return loom_mode1_segments_equal(&scene->load_segments[index],
                                     &resident->load_segments[index]);
}

static void loom_mode1_skip_resident_segments(void)
{
    const LoomMode1Scene *scene;

    scene = loom_mode1_state.scene;
    while (loom_mode1_state.load_segment < scene->load_segment_count &&
           loom_mode1_segment_is_resident(loom_mode1_state.load_segment) !=
               LOOM_FALSE) {
        ++loom_mode1_state.load_segment;
    }
    if (loom_mode1_state.load_segment >= scene->load_segment_count) {
        loom_mode1_state.ready = LOOM_TRUE;
        ++loom_mode1_debug_epoch;
    }
}

static void loom_mode1_advance_load(void)
{
    loom_mode1_state.load_segment = loom_mode1_state.pending_segment;
    loom_mode1_state.load_offset = loom_mode1_state.pending_offset;
    loom_mode1_skip_resident_segments();
}

static void loom_mode1_observe_boundary(const LoomFrameBoundary *boundary)
{
    if (loom_mode1_state.pending_load != LOOM_FALSE &&
        boundary->presentation == LOOM_PRESENTATION_NEW_COMMIT &&
        boundary->presented_commit_id ==
            loom_mode1_state.pending_commit_id) {
        loom_mode1_advance_load();
        loom_mode1_state.pending_load = LOOM_FALSE;
    }
}

loom_u8 loom_mode1_debug_epoch;

/* The last tick's layer scroll, valid while the scene has no drifting
 * layer; reset with the scene. */
typedef struct LoomMode1ScrollCache {
    loom_s16 x[4];
    loom_s16 y[4];
    loom_s16 camera_x;
    loom_s16 camera_y;
    loom_u8 has_bg2;
    loom_u8 has_bg3;
    loom_u8 valid;
} LoomMode1ScrollCache;

static LoomMode1ScrollCache loom_mode1_scroll_cache;

/* Set when a sprite's pose is written (activation, set_sprite_pose, a part's
 * pose); oam.asm clears it once the sprite's tile and attributes are in the
 * OAM shadow, and while it stays clear a still-staged sprite rewrites only
 * its position. Exported for oam.asm. */
loom_u8 loom_mode1_pose_dirty[LOOM_FRAME_OAM_CAPACITY];

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
/* oam.asm: the scroll block below for a scene with no drifting layer; it
 * returns has_bg2 | has_bg3 << 1, or 0xffff to leave the block to C. It
 * reads the cache and the layer records by offset. */
LOOM_STATIC_ASSERT(loom_mode1_scroll_cache_matches_asm,
                   sizeof(LoomMode1ScrollCache) == 24u);
/* oam.asm: the display block after the scroll (backdrop, brightness, main
 * layers, sprite size, raster binding, forced blank). bits carries has_bg2
 * 1, has_bg3 2, ready 4, raster_enabled 8 and the brightness in the high
 * byte; the result is lit 1, colour math due 2, raster drive due 4. Both
 * structs are read by offset; 816-tcc aligns pointers to four bytes, so the
 * scene is 68 (color_math_mode at 65) and these sizes pin the layout. */
LOOM_STATIC_ASSERT(loom_mode1_scene_matches_asm,
                   sizeof(LoomMode1Scene) == 68u);
LOOM_STATIC_ASSERT(loom_display_state_matches_asm,
                   sizeof(LoomDisplayState) == 31u ||
                       sizeof(LoomDisplayState) == 32u);
loom_u16 loom_pvs_mode1_display(LoomDisplayState *display,
                                LoomRasterBinding *raster,
                                const LoomMode1Scene *scene, loom_u16 bits);
loom_u16 loom_pvs_mode1_scroll(LoomDisplayState *display,
                               LoomMode1ScrollCache *cache,
                               const LoomMode1Layer *layers,
                               loom_u16 layer_count, loom_s16 camera_x,
                               loom_s16 camera_y);
#endif

/* A scene loads on a dark screen, where a transfer that outlasts VBlank
 * loses nothing, so each commit carries as many segments as its jobs and
 * bytes hold: a room is lit in a dozen ticks rather than sixty. */
static LoomStatus loom_mode1_add_load_jobs(void)
{
    const LoomMode1Scene *scene;
    const LoomMode1LoadSegment *segment;
    LoomDmaJob job;
    LoomCommitId commit_id;
    loom_u16 budget;
    loom_u16 index;
    loom_u16 offset;
    loom_u16 used;
    loom_u8 room;
    LoomStatus status;

    scene = loom_mode1_state.scene;
    if (loom_mode1_state.load_segment >= scene->load_segment_count) {
        loom_mode1_state.ready = LOOM_TRUE;
        ++loom_mode1_debug_epoch;
        return LOOM_STATUS_OK;
    }
    (void)loom_frame_build_dma_space(&room);
    used = loom_frame_build_dma_bytes();
    if (room == 0u || used >= LOOM_FRAME_REQUIRED_DMA_BYTES_MAX) {
        return LOOM_STATUS_OK;
    }
    budget = (loom_u16)(LOOM_FRAME_REQUIRED_DMA_BYTES_MAX - used);
    index = loom_mode1_state.load_segment;
    offset = loom_mode1_state.load_offset;
    while (room != 0u && budget != 0u && index < scene->load_segment_count) {
        loom_u16 bytes;

        if (offset == 0u && loom_mode1_segment_is_resident(index) != LOOM_FALSE) {
            ++index;
            continue;
        }
        segment = &scene->load_segments[index];
        bytes = (loom_u16)(segment->byte_count - offset);
        if (bytes > budget) {
            bytes = budget;
        }
        if (segment->destination_kind == LOOM_DMA_DESTINATION_VRAM_COLUMN &&
            bytes > 64u) {
            bytes = 64u;
        }
        job.job_id = loom_mode1_state.next_job_id;
        job.source_handle = segment->source_handle;
        job.source_offset = (loom_u16)(segment->source_offset + offset);
        job.destination_offset =
            (loom_u16)(segment->destination_offset + offset);
        job.byte_count = bytes;
        job.source_kind = LOOM_DMA_SOURCE_ROM_ASSET;
        job.destination_kind = segment->destination_kind;
        job.policy = LOOM_DMA_REQUIRED;
        job.reserved = 0u;
        status = loom_frame_build_add_dma(&job);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        ++loom_mode1_state.next_job_id;
        --room;
        budget = (loom_u16)(budget - bytes);
        offset = (loom_u16)(offset + bytes);
        if (offset == segment->byte_count) {
            ++index;
            offset = 0u;
        }
    }
    status = loom_frame_build_current_commit_id(&commit_id);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_mode1_state.pending_segment = index;
    loom_mode1_state.pending_offset = offset;
    loom_mode1_state.pending_commit_id = commit_id;
    loom_mode1_state.pending_load = LOOM_TRUE;
    return LOOM_STATUS_OK;
}

static LoomStatus loom_mode1_stream_job(loom_u16 buffer_word,
                                        loom_u16 destination_byte,
                                        loom_u16 byte_count,
                                        loom_u8 destination_kind)
{
    LoomDmaJob job;

    job.job_id = (loom_u16)(0x8000u | loom_mode1_state.stream_job_id);
    job.source_handle = LOOM_MODE1_STREAM_BLOCK_HANDLE;
    job.source_offset = (loom_u16)(buffer_word * 2u);
    job.destination_offset = destination_byte;
    job.byte_count = byte_count;
    job.source_kind = LOOM_DMA_SOURCE_WRAM_BLOCK;
    job.destination_kind = destination_kind;
    job.policy = LOOM_DMA_REQUIRED;
    job.reserved = 0u;
    ++loom_mode1_state.stream_job_id;
    return loom_frame_build_add_dma(&job);
}

/* Expands scene metatile column `column` over the loaded rows into two
 * 32-word tile columns and DMAs them with a 32-word VRAM step. */
static LoomStatus loom_mode1_stream_column_strip(loom_u16 column)
{
    const LoomMode1Stream *stream;
    loom_u16 *left;
    loom_u16 *right;
    loom_u16 tile_column;
    loom_u16 index;
    LoomStatus status;

    stream = loom_mode1_state.scene->stream;
    left = &loom_mode1_stream_buffer[0];
    right = &loom_mode1_stream_buffer[32];
    /* One pass down the column: the grid pointer steps a row at a time and
     * each cell's four words copy from its table entry, with no call, no
     * multiply and one bounds test per cell (PERF-003: this strip was 300
     * scanlines of calls, and a scrolling tick paid it every column). */
    {
        loom_u16 row;
        loom_u16 rows_left;
        const loom_u8 *cell;
        loom_u16 blank;
        loom_u8 count;
        loom_u16 width;
        loom_u16 height;

        row = loom_mode1_state.stream_row;
        blank = stream->blank_word;
        count = stream->metatile_count;
        width = stream->width_metatiles;
        height = stream->height_metatiles;
        cell = stream->grid + (loom_u16)(row * width) + column;
        for (rows_left = LOOM_MODE1_STREAM_WINDOW_ROWS; rows_left != 0u;
             --rows_left) {
            loom_u8 slot;
            loom_u8 id;

            slot = (loom_u8)((row & 15u) * 2u);
            id = LOOM_MODE1_STREAM_BLANK;
            if (column < width && row < height) {
                id = *cell;
            }
            if (id == LOOM_MODE1_STREAM_BLANK || id >= count) {
                left[slot] = blank;
                right[slot] = blank;
                left[slot + 1u] = blank;
                right[slot + 1u] = blank;
            } else {
                const loom_u16 *entry;

                entry = stream->table + (loom_u16)((loom_u16)id << 2);
                left[slot] = entry[0];
                right[slot] = entry[1];
                left[slot + 1u] = entry[2];
                right[slot + 1u] = entry[3];
            }
            ++row;
            cell += width;
        }
    }
    for (index = 0u; index < 2u; ++index) {
        loom_u16 slot;
        loom_u16 word;

        tile_column = (loom_u16)(column * 2u + index);
        slot = (loom_u16)(tile_column & 63u);
        word = (loom_u16)((LOOM_MODE1_BG1_MAP_VRAM_BYTE / 2u) +
                          (slot >> 5) * 1024u + (slot & 31u));
        status = loom_mode1_stream_job(
            (loom_u16)(index * 32u), (loom_u16)(word * 2u),
            (loom_u16)(LOOM_MODE1_STREAM_WINDOW_ROWS * 2u * 2u),
            LOOM_DMA_DESTINATION_VRAM_COLUMN);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    return LOOM_STATUS_OK;
}

/* Expands scene metatile row `row` over the loaded columns into two tile
 * rows split by screen block, using buffer words from `base`. */
static LoomStatus loom_mode1_stream_row_strip(loom_u16 row, loom_u16 base)
{
    const LoomMode1Stream *stream;
    loom_u16 *buffer;
    loom_u16 index;
    LoomStatus status;

    stream = loom_mode1_state.scene->stream;
    buffer = &loom_mode1_stream_buffer[base];
    /* One pass along the row, the grid pointer stepping a cell at a time
     * (PERF-003, as the column strip). */
    {
        loom_u16 column;
        const loom_u8 *cell;
        loom_u16 blank;
        loom_u8 count;
        loom_u16 width;
        loom_u8 in_row;

        column = loom_mode1_state.stream_column;
        blank = stream->blank_word;
        count = stream->metatile_count;
        width = stream->width_metatiles;
        in_row = (loom_u8)(row < stream->height_metatiles);
        cell = stream->grid + (loom_u16)(row * width) + column;
        for (index = 0u; index < LOOM_MODE1_STREAM_WINDOW_COLUMNS; ++index) {
            loom_u16 slot;
            loom_u16 at;
            loom_u8 id;

            slot = (loom_u16)((column * 2u) & 63u);
            /* Screen 0 words come first, then screen 1, for each tile row. */
            at = (loom_u16)((slot >> 5) * 32u + (slot & 31u));
            id = LOOM_MODE1_STREAM_BLANK;
            if (in_row != LOOM_FALSE && column < width) {
                id = *cell;
            }
            if (id == LOOM_MODE1_STREAM_BLANK || id >= count) {
                buffer[at] = blank;
                buffer[at + 1u] = blank;
                buffer[64u + at] = blank;
                buffer[64u + at + 1u] = blank;
            } else {
                const loom_u16 *entry;

                entry = stream->table + (loom_u16)((loom_u16)id << 2);
                buffer[at] = entry[0];
                buffer[at + 1u] = entry[1];
                buffer[64u + at] = entry[2];
                buffer[64u + at + 1u] = entry[3];
            }
            ++column;
            ++cell;
        }
    }
    for (index = 0u; index < 4u; ++index) {
        loom_u16 tile_row;
        loom_u16 word;

        tile_row = (loom_u16)(((row * 2u) & 31u) + (index >> 1));
        word = (loom_u16)((LOOM_MODE1_BG1_MAP_VRAM_BYTE / 2u) +
                          (index & 1u) * 1024u + tile_row * 32u);
        status = loom_mode1_stream_job(
            (loom_u16)(base + index * 32u), (loom_u16)(word * 2u), 64u,
            LOOM_DMA_DESTINATION_VRAM);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    return LOOM_STATUS_OK;
}

/* Keeps the hardware map window around the camera: up to one column and
 * two rows move per frame, enough for sixteen pixels of travel per tick. */
static LoomStatus loom_mode1_stream_update(void)
{
    const LoomMode1Stream *stream;
    loom_u16 camera_column;
    loom_u16 camera_row;
    loom_u16 desired_column;
    loom_u16 desired_row;
    loom_u16 max_column;
    loom_u16 max_row;
    loom_u16 base;
    loom_u8 rows;
    LoomStatus status;

    stream = loom_mode1_state.scene->stream;
    if (stream == (const LoomMode1Stream *)0) {
        return LOOM_STATUS_OK;
    }
    camera_column = (loom_u16)((loom_u16)loom_mode1_state.camera_x / 16u);
    camera_row = (loom_u16)((loom_u16)loom_mode1_state.camera_y / 16u);
    max_column = loom_mode1_stream_max_column(stream);
    max_row = loom_mode1_stream_max_row(stream);
    desired_column = camera_column > LOOM_MODE1_STREAM_LOOK_BEHIND
                         ? (loom_u16)(camera_column -
                                      LOOM_MODE1_STREAM_LOOK_BEHIND)
                         : 0u;
    if (desired_column > max_column) {
        desired_column = max_column;
    }
    /* The map holds one spare row: keep it above while moving up, below
     * otherwise, so the next row is resident before the camera reaches it. */
    desired_row = camera_row;
    if (loom_mode1_state.camera_y < loom_mode1_state.stream_last_camera_y &&
        desired_row > 0u) {
        --desired_row;
    }
    if (desired_row > max_row) {
        desired_row = max_row;
    }
    loom_mode1_state.stream_last_camera_y = loom_mode1_state.camera_y;

    if (desired_column > loom_mode1_state.stream_column) {
        status = loom_mode1_stream_column_strip(
            (loom_u16)(loom_mode1_state.stream_column +
                       LOOM_MODE1_STREAM_WINDOW_COLUMNS));
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        ++loom_mode1_state.stream_column;
    } else if (desired_column < loom_mode1_state.stream_column) {
        status = loom_mode1_stream_column_strip(
            (loom_u16)(loom_mode1_state.stream_column - 1u));
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        --loom_mode1_state.stream_column;
    }
    base = LOOM_MODE1_STREAM_COLUMN_WORDS;
    for (rows = 0u; rows < LOOM_MODE1_STREAM_ROWS_PER_FRAME; ++rows) {
        if (desired_row > loom_mode1_state.stream_row) {
            status = loom_mode1_stream_row_strip(
                (loom_u16)(loom_mode1_state.stream_row +
                           LOOM_MODE1_STREAM_WINDOW_ROWS),
                base);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            ++loom_mode1_state.stream_row;
        } else if (desired_row < loom_mode1_state.stream_row) {
            status = loom_mode1_stream_row_strip(
                (loom_u16)(loom_mode1_state.stream_row - 1u), base);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
            --loom_mode1_state.stream_row;
        } else {
            break;
        }
        base = (loom_u16)(base + LOOM_MODE1_STREAM_ROW_WORDS);
    }
    return LOOM_STATUS_OK;
}

static LoomMode1SpriteBatch loom_mode1_batch;

static LoomStatus loom_mode1_add_sprites(void)
{
    loom_mode1_batch.sprites = loom_mode1_state.scene->sprites;
    loom_mode1_batch.poses = loom_mode1_state.sprite_pose;
    loom_mode1_batch.visible = loom_mode1_state.sprite_visible;
    loom_mode1_batch.world_x = loom_mode1_state.sprite_world_x;
    loom_mode1_batch.world_y = loom_mode1_state.sprite_world_y;
    loom_mode1_batch.camera_x = loom_mode1_state.camera_x;
    loom_mode1_batch.camera_y = loom_mode1_state.camera_y;
    loom_mode1_batch.count = loom_mode1_state.scene->sprite_count;
    loom_mode1_batch.reserved = 0u;
#if defined(LOOM_MODE1_SPRITE_BATCH)
    return loom_pvs_oam_batch(&loom_mode1_batch);
#else
    {
        const LoomMode1Sprite *sprite;
        const LoomMode1SpritePose *pose;
        const loom_u8 *visible;
        const loom_s16 *world_x;
        const loom_s16 *world_y;
        loom_u8 remaining;

        sprite = loom_mode1_batch.sprites;
        pose = loom_mode1_batch.poses;
        visible = loom_mode1_batch.visible;
        world_x = loom_mode1_batch.world_x;
        world_y = loom_mode1_batch.world_y;
        for (remaining = loom_mode1_batch.count; remaining != 0u;
             --remaining, ++sprite, ++pose, ++visible, ++world_x,
             ++world_y) {
            LoomOamEntry *entry;
            loom_s16 screen_x;
            loom_s16 screen_y;

            if (*visible == LOOM_FALSE) {
                continue;
            }
            screen_x = (loom_s16)(*world_x - loom_mode1_batch.camera_x -
                                  pose->pivot_x);
            if (screen_x <= (loom_s16)(0 - (loom_s16)pose->width) ||
                screen_x >= (loom_s16)LOOM_MODE1_VIEW_WIDTH) {
                continue;
            }
            screen_y = (loom_s16)(*world_y - loom_mode1_batch.camera_y -
                                  pose->pivot_y);
            if (screen_y <= (loom_s16)(0 - (loom_s16)pose->height) ||
                screen_y >= (loom_s16)LOOM_MODE1_VIEW_HEIGHT) {
                continue;
            }
            entry = loom_frame_build_reserve_oam();
            if (entry == (LoomOamEntry *)0) {
                return LOOM_STATUS_CAPACITY;
            }
            entry->x = screen_x;
            entry->y = screen_y;
            entry->tile_index = pose->tile_index;
            entry->slot = sprite->slot;
            entry->palette = pose->palette;
            entry->priority = sprite->priority;
            entry->size = pose->size;
            /* The pose adds the animation's mirror to the authored flips. */
            entry->flags = (loom_u8)(sprite->flags | pose->flags);
            entry->reserved = 0u;
        }
        return LOOM_STATUS_OK;
    }
#endif
}

static LoomStatus loom_mode1_reset_scene(const LoomMode1Scene *scene)
{
    const LoomMode1Scene *resident_scene;
    loom_u8 index;

    resident_scene = loom_mode1_state.ready != LOOM_FALSE
                         ? loom_mode1_state.scene
                         : (const LoomMode1Scene *)0;
    loom_mode1_state.resident_scene = resident_scene;
    loom_mode1_state.scene = scene;
    loom_mode1_scroll_cache.valid = LOOM_FALSE;
#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
    /* camera_x and camera_y sit together: the camera tick writes both. */
    loom_pvs_mode1_bind(loom_mode1_state.sprite_world_x,
                        loom_mode1_state.sprite_world_y,
                        loom_mode1_state.sprite_part_count,
                        scene != (const LoomMode1Scene *)0 ? scene->sprite_count : 0u,
                        loom_mode1_state.slot_index, &loom_mode1_state.camera_x,
                        scene != (const LoomMode1Scene *)0 ? scene->camera_min_x : 0,
                        scene != (const LoomMode1Scene *)0 ? scene->camera_min_y : 0,
                        scene != (const LoomMode1Scene *)0 ? scene->camera_max_x : 0,
                        scene != (const LoomMode1Scene *)0 ? scene->camera_max_y : 0);
#endif
    loom_mode1_state.load_segment = 0u;
    loom_mode1_state.load_offset = 0u;
    loom_mode1_state.next_job_id = 0u;
    {
        loom_u8 index;

        for (index = 0u; index < LOOM_MODE1_TILE_ANIMATIONS_MAX; ++index) {
            loom_mode1_state.animation_frame[index] = 0u;
            loom_mode1_state.animation_elapsed[index] = 0u;
        }
        for (index = 0u; index < LOOM_MODE1_PALETTE_CYCLES_MAX; ++index) {
            loom_mode1_state.cycle_step[index] = 0u;
            loom_mode1_state.cycle_elapsed[index] = 0u;
        }
        loom_mode1_state.animation_due = 0u;
        loom_mode1_state.animation_next = 0u;
        loom_mode1_state.cycle_due = 0u;
        loom_mode1_state.cycle_next = 0u;
    }
    loom_mode1_state.pending_commit_id = LOOM_COMMIT_NONE;
    loom_mode1_state.pending_load = LOOM_FALSE;
    loom_mode1_state.raster_enabled = LOOM_TRUE;
    loom_mode1_state.raster_buffer = 0u;
    loom_mode1_state.wave_phase = 0u;
    ++loom_mode1_debug_epoch;
    loom_mode1_state.ready = scene->load_segment_count == 0u
                                 ? LOOM_TRUE
                                 : LOOM_FALSE;
    loom_mode1_skip_resident_segments();
    loom_mode1_state.camera_x = loom_mode1_clamp(
        scene->initial_camera_x, scene->camera_min_x, scene->camera_max_x);
    loom_mode1_state.camera_y = loom_mode1_clamp(
        scene->initial_camera_y, scene->camera_min_y, scene->camera_max_y);
    loom_mode1_state.stream_column =
        scene->stream != (const LoomMode1Stream *)0
            ? scene->stream->initial_column
            : 0u;
    loom_mode1_state.stream_row =
        scene->stream != (const LoomMode1Stream *)0
            ? scene->stream->initial_row
            : 0u;
    loom_mode1_state.stream_last_camera_y = loom_mode1_state.camera_y;
    loom_mode1_state.stream_job_id = 0u;
    for (index = 0u; index < LOOM_MODE1_LAYER_CAPACITY; ++index) {
        loom_mode1_state.layer_auto_x[index] = 0;
        loom_mode1_state.layer_auto_y[index] = 0;
        loom_mode1_state.layer_auto_frac_x[index] = 0;
        loom_mode1_state.layer_auto_frac_y[index] = 0;
    }
    for (index = 0u; index < LOOM_FRAME_OAM_CAPACITY; ++index) {
        loom_mode1_state.sprite_world_x[index] = 0;
        loom_mode1_state.sprite_world_y[index] = 0;
        loom_mode1_state.sprite_visible[index] = LOOM_FALSE;
        loom_mode1_state.sprite_part_count[index] = 0u;
        loom_mode1_state.sprite_pose[index].pivot_x = 0;
        loom_mode1_state.sprite_pose[index].pivot_y = 0;
        loom_mode1_state.sprite_pose[index].tile_index = 0u;
        loom_mode1_state.sprite_pose[index].palette = 0u;
        loom_mode1_state.sprite_pose[index].size = LOOM_OAM_SIZE_SMALL;
        loom_mode1_state.sprite_pose[index].width = 0u;
        loom_mode1_state.sprite_pose[index].height = 0u;
        loom_mode1_state.sprite_pose[index].flags = 0u;
        loom_mode1_pose_dirty[index] = LOOM_TRUE;
    }
    for (index = 0u; index < LOOM_OAM_SLOT_MAX; ++index) {
        loom_mode1_state.slot_index[index] = 0xffu;
    }
    loom_mode1_state.slot_index[LOOM_OAM_SLOT_MAX] = 0xffu;
    {
        const LoomMode1Sprite *sprite;
        LoomMode1SpritePose *pose;

        sprite = scene->sprites;
        pose = loom_mode1_state.sprite_pose;
        for (index = 0u; index < scene->sprite_count; ++index, ++sprite, ++pose) {
            loom_mode1_state.slot_index[sprite->slot] = index;
            loom_mode1_state.sprite_visible[index] = LOOM_TRUE;
            loom_mode1_state.sprite_part_count[index] = sprite->part_count;
            loom_mode1_state.sprite_world_x[index] = sprite->world_x;
            loom_mode1_state.sprite_world_y[index] = sprite->world_y;
            pose->pivot_x = sprite->pivot_x;
            pose->pivot_y = sprite->pivot_y;
            pose->tile_index = sprite->tile_index;
            pose->palette = sprite->palette;
            pose->size = sprite->size;
            pose->width = sprite->width;
            pose->height = sprite->height;
            pose->flags = 0u;
        }
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_mode1_initialize(void)
{
    LoomStatus status;

    /* The guarded runtime root owns one-time initialization. Do not rely on
     * target .bss clearing for private module sentinels. */
    loom_mode1_state.scene = (const LoomMode1Scene *)0;
    loom_mode1_state.resident_scene = (const LoomMode1Scene *)0;
    loom_mode1_state.brightness = 15u;
    loom_mode1_state.raster_enabled = LOOM_FALSE;
    loom_mode1_state.ready = LOOM_TRUE;
    ++loom_mode1_debug_epoch;
    loom_mode1_state.initialized = LOOM_TRUE;
    if (loom_generated_mode1_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    status = loom_mode1_activate_scene(
        &loom_generated_mode1_initial_scene);
    if (status != LOOM_STATUS_OK) {
        loom_mode1_state.initialized = LOOM_FALSE;
    }
    return status;
}

LoomStatus loom_mode1_activate_scene(const LoomMode1Scene *scene)
{
    LoomStatus status;

    if (loom_mode1_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    status = loom_mode1_validate_scene(scene);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    return loom_mode1_reset_scene(scene);
}

/* The shift for each denominator a layer may author (terms run 1 through 8),
 * or 0xff for one that is not a power of two. A divide is a helper loop on
 * the 65816 and this ran twice per layer per frame; a shift is not. */
static const loom_u8 loom_mode1_denominator_shift[LOOM_MODE1_SCROLL_TERM_MAX +
                                                  1u] = {
    0xffu, 0u, 1u, 0xffu, 2u, 0xffu, 0xffu, 0xffu, 3u};

/* camera * numerator / denominator with the runtime's integer truncation. */
static loom_s16 loom_mode1_scaled(loom_s16 value,
                                  loom_u8 numerator,
                                  loom_u8 denominator)
{
    loom_u16 magnitude;
    loom_u16 scaled;
    loom_u8 shift;

    if (numerator == denominator) {
        return value;
    }
    magnitude = value < 0 ? (loom_u16)(-value) : (loom_u16)value;
    if (numerator != 1u) {
        magnitude = (loom_u16)(magnitude * numerator);
    }
    shift = denominator <= LOOM_MODE1_SCROLL_TERM_MAX
                ? loom_mode1_denominator_shift[denominator]
                : 0xffu;
    if (shift != 0xffu) {
        scaled = (loom_u16)(magnitude >> shift);
    } else {
        scaled = (loom_u16)(magnitude / denominator);
    }
    return value < 0 ? (loom_s16)(-(loom_s16)scaled) : (loom_s16)scaled;
}

/* Adds one frame of drift in 1/256 pixel, wrapping at the map's extent. */
static void loom_mode1_advance_auto_scroll(loom_s16 subpixels_per_frame,
                                           loom_s16 *pixels,
                                           loom_s16 *fraction,
                                           loom_s16 wrap)
{
    loom_s16 whole;
    loom_s16 sub;
    loom_s16 total;

    if (subpixels_per_frame == 0) {
        return;
    }
    if (subpixels_per_frame >= 0) {
        whole = (loom_s16)(subpixels_per_frame / 256);
        sub = (loom_s16)(subpixels_per_frame % 256);
    } else {
        whole = (loom_s16)(-((-subpixels_per_frame) / 256));
        sub = (loom_s16)(-((-subpixels_per_frame) % 256));
    }
    total = (loom_s16)(*fraction + sub);
    if (total < 0) {
        total = (loom_s16)(total + 256);
        whole = (loom_s16)(whole - 1);
    } else if (total >= 256) {
        total = (loom_s16)(total - 256);
        whole = (loom_s16)(whole + 1);
    }
    *fraction = total;
    *pixels = (loom_s16)(*pixels + whole);
    while (*pixels < 0) {
        *pixels = (loom_s16)(*pixels + wrap);
    }
    while (*pixels >= wrap) {
        *pixels = (loom_s16)(*pixels - wrap);
    }
}

/* Advances every tile animation and palette cycle by one logical tick and
 * marks the ones whose next frame or step is due. Cheap on 816-tcc: one
 * counter compare per entry, no per-cell work. */
static void loom_mode1_advance_animations(void)
{
    const LoomMode1Scene *scene;
    loom_u8 index;

    scene = loom_mode1_state.scene;
    for (index = 0u; index < scene->tile_animation_count; ++index) {
        const LoomMode1TileAnimation *animation;
        loom_u8 frame;

        animation = &scene->tile_animations[index];
        frame = loom_mode1_state.animation_frame[index];
        ++loom_mode1_state.animation_elapsed[index];
        if (loom_mode1_state.animation_elapsed[index] <
            animation->durations[frame]) {
            continue;
        }
        loom_mode1_state.animation_elapsed[index] = 0u;
        ++frame;
        if (frame >= animation->frame_count) {
            frame = 0u;
        }
        loom_mode1_state.animation_frame[index] = frame;
        loom_mode1_state.animation_due |= (loom_u8)(1u << index);
        ++loom_mode1_debug_epoch;
    }
    for (index = 0u; index < scene->palette_cycle_count; ++index) {
        const LoomMode1PaletteCycle *cycle;
        loom_u8 step;

        cycle = &scene->palette_cycles[index];
        ++loom_mode1_state.cycle_elapsed[index];
        if (loom_mode1_state.cycle_elapsed[index] < cycle->period_ticks) {
            continue;
        }
        loom_mode1_state.cycle_elapsed[index] = 0u;
        step = (loom_u8)(loom_mode1_state.cycle_step[index] + 1u);
        if (step >= cycle->step_count) {
            step = 0u;
        }
        loom_mode1_state.cycle_step[index] = step;
        loom_mode1_state.cycle_due |= (loom_u8)(1u << index);
        ++loom_mode1_debug_epoch;
    }
}

/* Stages at most one due tile animation frame (all its runs, so a metatile
 * never shows half a frame) and one due palette cycle step. Anything else
 * due keeps its bit for a later tick: that keeps a streamed room with a UI
 * inside the commit's sixteen jobs and 1,024 bytes. */
static LoomStatus loom_mode1_add_animation_jobs(void)
{
    const LoomMode1Scene *scene;
    LoomDmaJob job;
    LoomStatus status;
    loom_u8 count;
    loom_u8 index;
    loom_u8 tries;

    scene = loom_mode1_state.scene;
    job.source_kind = LOOM_DMA_SOURCE_ROM_ASSET;
    job.policy = LOOM_DMA_REQUIRED;
    job.reserved = 0u;
    count = scene->tile_animation_count;
    index = loom_mode1_state.animation_next;
    for (tries = 0u; tries < count; ++tries) {
        const LoomMode1TileAnimation *animation;
        loom_u16 frame_base;
        loom_u8 run;

        if (index >= count) {
            index = 0u;
        }
        if ((loom_mode1_state.animation_due & (loom_u8)(1u << index)) == 0u) {
            ++index;
            continue;
        }
        animation = &scene->tile_animations[index];
        frame_base = (loom_u16)(animation->frame_stride *
                                loom_mode1_state.animation_frame[index]);
        for (run = 0u; run < animation->run_count; ++run) {
            const LoomMode1TileRun *record;

            record = &animation->runs[run];
            job.job_id = (loom_u16)(0x4000u | (loom_u16)(index << 4) | run);
            job.source_handle = animation->frames;
            job.source_offset = (loom_u16)(frame_base + record->source_offset);
            job.destination_offset = record->destination_offset;
            job.byte_count = record->byte_count;
            job.destination_kind = LOOM_DMA_DESTINATION_VRAM;
            status = loom_frame_build_add_dma(&job);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
        loom_mode1_state.animation_due &= (loom_u8)~(loom_u8)(1u << index);
        loom_mode1_state.animation_next = (loom_u8)(index + 1u);
        break;
    }
    count = scene->palette_cycle_count;
    index = loom_mode1_state.cycle_next;
    for (tries = 0u; tries < count; ++tries) {
        const LoomMode1PaletteCycle *cycle;

        if (index >= count) {
            index = 0u;
        }
        if ((loom_mode1_state.cycle_due & (loom_u8)(1u << index)) == 0u) {
            ++index;
            continue;
        }
        cycle = &scene->palette_cycles[index];
        job.job_id = (loom_u16)(0x4800u | index);
        job.source_handle = cycle->steps;
        job.source_offset = (loom_u16)(cycle->byte_count *
                                       loom_mode1_state.cycle_step[index]);
        job.destination_offset = cycle->destination_offset;
        job.byte_count = cycle->byte_count;
        job.destination_kind = LOOM_DMA_DESTINATION_CGRAM;
        status = loom_frame_build_add_dma(&job);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        loom_mode1_state.cycle_due &= (loom_u8)~(loom_u8)(1u << index);
        loom_mode1_state.cycle_next = (loom_u8)(index + 1u);
        break;
    }
    return LOOM_STATUS_OK;
}

loom_u8 loom_mode1_tile_animation_frame(loom_u8 index)
{
    if (index >= LOOM_MODE1_TILE_ANIMATIONS_MAX) {
        return 0u;
    }
    return loom_mode1_state.animation_frame[index];
}

loom_u8 loom_mode1_palette_cycle_step(loom_u8 index)
{
    if (index >= LOOM_MODE1_PALETTE_CYCLES_MAX) {
        return 0u;
    }
    return loom_mode1_state.cycle_step[index];
}

/* The one blended layer: translucent moves it to the sub screen and adds it
 * at half under every main layer and the backdrop; darken subtracts a grey
 * from it alone. The cartridge UI's background never joins (the planner
 * keeps it out of the mask), and OBJ joins only for palettes 4..7, which
 * is the console's rule. */
static void loom_mode1_apply_color_math(LoomDisplayState *display)
{
    const LoomMode1Scene *scene;
    loom_u8 amount;

    scene = loom_mode1_state.scene;
    if (scene->color_math_mode == LOOM_MODE1_COLOR_MATH_TRANSLUCENT) {
        display->main_layers =
            (loom_u8)(display->main_layers & (loom_u8)(~scene->color_math_layers));
        display->sub_layers = scene->color_math_layers;
        display->color_math_layers =
            (loom_u8)((display->main_layers & (loom_u8)(LOOM_LAYER_BG1 | LOOM_LAYER_BG2 |
                                                          LOOM_LAYER_OBJ)) |
                      LOOM_LAYER_BACKDROP);
        display->color_math_flags = LOOM_COLOR_MATH_HALF;
    } else if (scene->color_math_mode == LOOM_MODE1_COLOR_MATH_DARKEN) {
        amount = scene->color_math_amount;
        display->color_math_layers = scene->color_math_layers;
        display->color_math_flags =
            (loom_u8)(LOOM_COLOR_MATH_SUBTRACT | LOOM_COLOR_MATH_USE_FIXED_COLOR);
        display->fixed_color =
            (loom_u16)((loom_u16)amount | ((loom_u16)amount << 5) |
                       ((loom_u16)amount << 10));
    }
}

/* What a resident raster program asks of the frame: the fixed gradient's
 * colour math, a scroll-band table rebuilt from the camera into the buffer
 * the hardware is not reading, or a wave phase carried in the binding. */
static void loom_mode1_drive_raster(LoomDisplayState *display,
                                    LoomRasterBinding *raster)
{
    const LoomMode1RasterParams *params;
    loom_u8 *table;
    loom_u8 index;
    loom_u8 layer;
    loom_s16 base;
    loom_s16 scroll;
    loom_u16 wavelength;
    loom_s16 phase;

    params = loom_mode1_state.scene->raster_params;
    if (params == (const LoomMode1RasterParams *)0) {
        return;
    }
    if (params->kind == LOOM_MODE1_RASTER_FIXED_COLOR) {
        /* The v0 gradient adds blue to BG1 through the fixed colour; the
         * backdrop gradient asks nothing of the display. */
        display->color_math_layers = LOOM_LAYER_BG1;
        display->color_math_flags = LOOM_COLOR_MATH_USE_FIXED_COLOR;
        return;
    }
    if (params->kind == LOOM_MODE1_RASTER_SCROLL_BANDS) {
        /* The auto-scroll of the layer the bands move, so a drifting sky
         * keeps drifting under its bands. */
        base = 0;
        for (layer = 0u; layer < loom_mode1_state.scene->layer_count; ++layer) {
            if (loom_mode1_state.scene->layers[layer].background == params->layer) {
                base = loom_mode1_state.layer_auto_x[layer];
            }
        }
        table = loom_mode1_raster_tables[loom_mode1_state.raster_buffer];
        for (index = 0u; index < params->band_count; ++index) {
            scroll = (loom_s16)(loom_mode1_scaled(loom_mode1_state.camera_x,
                                                  params->band_numerators[index],
                                                  params->band_denominators[index]) +
                                base);
            table[index * 3u] = params->band_lines[index];
            table[index * 3u + 1u] = (loom_u8)((loom_u16)scroll & 0x00ffu);
            table[index * 3u + 2u] = (loom_u8)(((loom_u16)scroll >> 8) & 0x00ffu);
        }
        table[params->band_count * 3u] = 0u;
        raster->state = (LoomRasterStateHandle)loom_mode1_state.raster_buffer;
        loom_mode1_state.raster_buffer = (loom_u8)(loom_mode1_state.raster_buffer ^ 1u);
        return;
    }
    if (params->kind == LOOM_MODE1_RASTER_WAVE) {
        wavelength = params->wave_wavelength;
        phase = (loom_s16)((loom_s16)loom_mode1_state.wave_phase + params->wave_speed);
        while (phase < 0) {
            phase = (loom_s16)(phase + (loom_s16)wavelength);
        }
        while (phase >= (loom_s16)wavelength) {
            phase = (loom_s16)(phase - (loom_s16)wavelength);
        }
        loom_mode1_state.wave_phase = (loom_u16)phase;
        raster->state = (LoomRasterStateHandle)loom_mode1_state.wave_phase;
    }
}

LoomStatus loom_mode1_build_frame(const LoomFrameBoundary *boundary)
{
    LoomDisplayState *display;
    LoomRasterBinding *raster;
    loom_u8 layer;
    loom_u8 has_bg2;
    loom_u8 has_bg3;
    loom_u8 lit;
    loom_u8 scrolled;
    loom_u8 blocked;
    LoomStatus status;

    if (loom_mode1_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (boundary == (const LoomFrameBoundary *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_OK;
    }
    display = loom_frame_build_display();
    raster = loom_frame_build_raster();
    if (display == (LoomDisplayState *)0 ||
        raster == (LoomRasterBinding *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    LOOM_RUNTIME_TRACE_MARK(0u);
    loom_mode1_observe_boundary(boundary);
    /* The open commit starts from the default display: every scroll is
     * zero, so only the layers this scene drives are written. */
    /* A still camera over layers that do not drift scrolls exactly as it
     * did last tick: the scaled parallax offsets are reused rather than
     * recomputed (a multiply and a divide per layer and axis on 816-tcc). */
    scrolled = LOOM_FALSE;
#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
    {
        loom_u16 bits;

        bits = loom_pvs_mode1_scroll(display, &loom_mode1_scroll_cache,
                                     loom_mode1_state.scene->layers,
                                     loom_mode1_state.scene->layer_count,
                                     loom_mode1_state.camera_x,
                                     loom_mode1_state.camera_y);
        if (bits != 0xffffu) {
            has_bg2 = (loom_u8)(bits & 1u);
            has_bg3 = (loom_u8)((bits >> 1) & 1u);
            scrolled = LOOM_TRUE;
        }
    }
#endif
    if (scrolled != LOOM_FALSE) {
        /* The assembly did the block. */
    } else if (loom_mode1_scroll_cache.valid != LOOM_FALSE &&
        loom_mode1_scroll_cache.camera_x == loom_mode1_state.camera_x &&
        loom_mode1_scroll_cache.camera_y == loom_mode1_state.camera_y) {
        /* Written out: an indexed store through a pointer is a multiply
         * and an add per element on 816-tcc, about 340 instructions for
         * this copy as a loop. */
        display->bg_scroll_x[0] = loom_mode1_scroll_cache.x[0];
        display->bg_scroll_x[1] = loom_mode1_scroll_cache.x[1];
        display->bg_scroll_x[2] = loom_mode1_scroll_cache.x[2];
        display->bg_scroll_x[3] = loom_mode1_scroll_cache.x[3];
        display->bg_scroll_y[0] = loom_mode1_scroll_cache.y[0];
        display->bg_scroll_y[1] = loom_mode1_scroll_cache.y[1];
        display->bg_scroll_y[2] = loom_mode1_scroll_cache.y[2];
        display->bg_scroll_y[3] = loom_mode1_scroll_cache.y[3];
        has_bg2 = loom_mode1_scroll_cache.has_bg2;
        has_bg3 = loom_mode1_scroll_cache.has_bg3;
    } else {
    loom_u8 drifts;

    drifts = LOOM_FALSE;
    display->bg_scroll_x[0] = loom_mode1_state.camera_x;
    display->bg_scroll_y[0] = loom_mode1_state.camera_y;
    has_bg2 = LOOM_FALSE;
    has_bg3 = LOOM_FALSE;
    for (layer = 0u; layer < loom_mode1_state.scene->layer_count; ++layer) {
        const LoomMode1Layer *record;
        loom_u8 background;

        record = &loom_mode1_state.scene->layers[layer];
        background = record->background;
        if (record->auto_scroll_x != 0 || record->auto_scroll_y != 0) {
            drifts = LOOM_TRUE;
        }
        if (record->auto_scroll_x != 0) {
            loom_mode1_advance_auto_scroll(
                record->auto_scroll_x,
                &loom_mode1_state.layer_auto_x[layer],
                &loom_mode1_state.layer_auto_frac_x[layer], 512);
        }
        if (record->auto_scroll_y != 0) {
            loom_mode1_advance_auto_scroll(
                record->auto_scroll_y,
                &loom_mode1_state.layer_auto_y[layer],
                &loom_mode1_state.layer_auto_frac_y[layer], 256);
        }
        display->bg_scroll_x[background] = (loom_s16)(
            loom_mode1_scaled(loom_mode1_state.camera_x,
                              record->scroll_x_numerator,
                              record->scroll_x_denominator) +
            loom_mode1_state.layer_auto_x[layer]);
        display->bg_scroll_y[background] = (loom_s16)(
            loom_mode1_scaled(loom_mode1_state.camera_y,
                              record->scroll_y_numerator,
                              record->scroll_y_denominator) +
            loom_mode1_state.layer_auto_y[layer]);
        if (background == 1u) {
            has_bg2 = LOOM_TRUE;
        } else if (background == 2u) {
            has_bg3 = LOOM_TRUE;
        }
    }
    loom_mode1_scroll_cache.x[0] = display->bg_scroll_x[0];
    loom_mode1_scroll_cache.x[1] = display->bg_scroll_x[1];
    loom_mode1_scroll_cache.x[2] = display->bg_scroll_x[2];
    loom_mode1_scroll_cache.x[3] = display->bg_scroll_x[3];
    loom_mode1_scroll_cache.y[0] = display->bg_scroll_y[0];
    loom_mode1_scroll_cache.y[1] = display->bg_scroll_y[1];
    loom_mode1_scroll_cache.y[2] = display->bg_scroll_y[2];
    loom_mode1_scroll_cache.y[3] = display->bg_scroll_y[3];
    loom_mode1_scroll_cache.camera_x = loom_mode1_state.camera_x;
    loom_mode1_scroll_cache.camera_y = loom_mode1_state.camera_y;
    loom_mode1_scroll_cache.has_bg2 = has_bg2;
    loom_mode1_scroll_cache.has_bg3 = has_bg3;
    /* A drifting layer moves every tick, so its scene never reuses. */
    loom_mode1_scroll_cache.valid = (loom_u8)(drifts == LOOM_FALSE);
    }
#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
    {
        loom_u16 todo;

        todo = loom_pvs_mode1_display(
            display, raster, loom_mode1_state.scene,
            (loom_u16)((loom_u16)has_bg2 | ((loom_u16)has_bg3 << 1) |
                       (loom_mode1_state.ready != LOOM_FALSE ? 4u : 0u) |
                       (loom_mode1_state.raster_enabled != LOOM_FALSE ? 8u
                                                                      : 0u) |
                       ((loom_u16)loom_mode1_state.brightness << 8)));
        lit = (loom_u8)(todo & 1u);
        if ((todo & 2u) != 0u) {
            loom_mode1_apply_color_math(display);
        }
        if ((todo & 4u) != 0u) {
            loom_mode1_drive_raster(display, raster);
        }
    }
#else
    lit = (loom_u8)(loom_mode1_state.ready != LOOM_FALSE &&
                    loom_mode1_state.brightness != 0u);
    display->backdrop_color = loom_mode1_state.scene->backdrop_color;
    display->brightness = loom_mode1_state.ready != LOOM_FALSE
                              ? loom_mode1_state.brightness
                              : 0u;
    display->main_layers =
        lit != LOOM_FALSE
            ? (loom_u8)(LOOM_LAYER_BG1 | LOOM_LAYER_OBJ |
                        (has_bg2 != LOOM_FALSE ? LOOM_LAYER_BG2 : 0u) |
                        (has_bg3 != LOOM_FALSE ? LOOM_LAYER_BG3 : 0u))
            : 0u;
    display->obj_size_pair = loom_mode1_state.scene->obj_size_pair;
    if (lit != LOOM_FALSE) {
        loom_mode1_apply_color_math(display);
    }
    *raster = loom_mode1_state.scene->raster;
    if (loom_mode1_state.raster_enabled == LOOM_FALSE ||
        loom_mode1_state.ready == LOOM_FALSE) {
        raster->program = LOOM_RASTER_PROGRAM_NONE;
        raster->state = LOOM_RASTER_STATE_NONE;
    }
    if (raster->program != LOOM_RASTER_PROGRAM_NONE) {
        loom_mode1_drive_raster(display, raster);
    }
    display->flags = lit != LOOM_FALSE ? 0u : LOOM_DISPLAY_FORCED_BLANK;
#endif
    LOOM_RUNTIME_TRACE_MARK(1u);
    if (loom_mode1_state.ready != LOOM_FALSE &&
        loom_generated_ui_enabled != LOOM_FALSE) {
        status = loom_ui_build_frame(boundary, display);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
        blocked = loom_ui_blocks_gameplay();
        if (blocked != LOOM_FALSE) {
            raster->program = LOOM_RASTER_PROGRAM_NONE;
            raster->state = LOOM_RASTER_STATE_NONE;
        }
    } else {
        blocked = loom_ui_blocks_gameplay();
    }
    LOOM_RUNTIME_TRACE_MARK(2u);
    if (loom_mode1_state.ready == LOOM_FALSE) {
        return loom_mode1_add_load_jobs();
    }
    status = loom_mode1_stream_update();
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    /* Water keeps moving under a pause menu: animations run whenever the
     * scene is resident, blocked gameplay or not. */
    if (loom_mode1_state.scene->tile_animation_count != 0u ||
        loom_mode1_state.scene->palette_cycle_count != 0u) {
        loom_mode1_advance_animations();
        status = loom_mode1_add_animation_jobs();
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    LOOM_RUNTIME_TRACE_MARK(3u);
    if (loom_generated_ui_enabled != LOOM_FALSE) {
        status = loom_ui_build_load();
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    if (blocked != LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    return loom_mode1_add_sprites();
}

loom_u8 *loom_mode1_stream_block(loom_u16 *bytes)
{
    if (bytes != (loom_u16 *)0) {
        *bytes = (loom_u16)(LOOM_MODE1_STREAM_BUFFER_WORDS * 2u);
    }
    return (loom_u8 *)loom_mode1_stream_buffer;
}

loom_u16 loom_mode1_stream_column(void)
{
    return loom_mode1_state.stream_column;
}

loom_u16 loom_mode1_stream_row(void)
{
    return loom_mode1_state.stream_row;
}

LoomStatus loom_mode1_set_brightness(loom_u8 brightness)
{
    if (loom_mode1_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (brightness > 15u) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    loom_mode1_state.brightness = brightness;
    ++loom_mode1_debug_epoch;
    return LOOM_STATUS_OK;
}

loom_u16 loom_mode1_wave_phase(void)
{
    return loom_mode1_state.wave_phase;
}

LoomStatus loom_mode1_set_raster_enabled(loom_u8 enabled)
{
    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    if (enabled != LOOM_FALSE && enabled != LOOM_TRUE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    loom_mode1_state.raster_enabled = enabled;
    ++loom_mode1_debug_epoch;
    return LOOM_STATUS_OK;
}

LoomStatus loom_mode1_set_camera(loom_s16 x, loom_s16 y)
{
    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    x = loom_mode1_clamp(x, loom_mode1_state.scene->camera_min_x,
                         loom_mode1_state.scene->camera_max_x);
    y = loom_mode1_clamp(y, loom_mode1_state.scene->camera_min_y,
                         loom_mode1_state.scene->camera_max_y);
    if (x != loom_mode1_state.camera_x || y != loom_mode1_state.camera_y) {
        loom_mode1_state.camera_x = x;
        loom_mode1_state.camera_y = y;
        ++loom_mode1_debug_epoch;
    }
    return LOOM_STATUS_OK;
}

static loom_s16 loom_mode1_sprite_index(loom_u8 slot)
{
    loom_u8 index;

    if (slot > LOOM_OAM_SLOT_MAX) {
        return (loom_s16)-1;
    }
    index = loom_mode1_state.slot_index[slot];
    if (index == 0xffu) {
        return (loom_s16)-1;
    }
    return (loom_s16)index;
}

LoomStatus loom_mode1_set_sprite_position(loom_u8 slot,
                                          loom_s16 world_x,
                                          loom_s16 world_y)
{
    loom_s16 index;

    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    index = loom_mode1_sprite_index(slot);
    if (index < 0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    {
        /* The parts of a metasprite follow the slot they belong to. Each
         * keeps its own pivot, so the same world position draws them in
         * their authored places around it. */
        loom_u8 remaining;
        loom_u8 cursor;

        cursor = (loom_u8)index;
        remaining = loom_mode1_state.sprite_part_count[cursor];
        loom_mode1_state.sprite_world_x[cursor] = world_x;
        loom_mode1_state.sprite_world_y[cursor] = world_y;
        while (remaining != 0u &&
               (loom_u16)(cursor + 1u) < loom_mode1_state.scene->sprite_count) {
            ++cursor;
            --remaining;
            loom_mode1_state.sprite_world_x[cursor] = world_x;
            loom_mode1_state.sprite_world_y[cursor] = world_y;
        }
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_mode1_sprite_position(loom_u8 slot,
                                      loom_s16 *world_x,
                                      loom_s16 *world_y)
{
    loom_s16 index;

    if (world_x == (loom_s16 *)0 || world_y == (loom_s16 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    index = loom_mode1_sprite_index(slot);
    if (index < 0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    *world_x = loom_mode1_state.sprite_world_x[(loom_u8)index];
    *world_y = loom_mode1_state.sprite_world_y[(loom_u8)index];
    return LOOM_STATUS_OK;
}

LoomStatus loom_mode1_set_sprite_pose(loom_u8 slot,
                                      const LoomMode1SpritePose *pose)
{
    LoomMode1SpritePose *target;
    loom_s16 index;
#if !defined(LOOM_TARGET_PVSNESLIB)
    LoomStatus status;
#endif

    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
#if !defined(LOOM_TARGET_PVSNESLIB)
    /* Poses come from plan-validated animation data on the console; the
     * host keeps the check for custom callers and the contract tests. */
    status = loom_mode1_validate_pose(pose);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
#endif
    index = loom_mode1_sprite_index(slot);
    if (index < 0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    /* One indexed address, not eight: 816-tcc multiplies per index. */
    target = &loom_mode1_state.sprite_pose[(loom_u8)index];
    loom_mode1_pose_dirty[(loom_u8)index] = LOOM_TRUE;
    target->pivot_x = pose->pivot_x;
    target->pivot_y = pose->pivot_y;
    target->tile_index = pose->tile_index;
    target->palette = pose->palette;
    target->size = pose->size;
    target->width = pose->width;
    target->height = pose->height;
    target->flags = pose->flags;
    return LOOM_STATUS_OK;
}

LoomStatus loom_mode1_set_sprite_part_pose(loom_u8 slot,
                                           loom_u8 part,
                                           const LoomMode1SpritePose *pose)
{
    LoomMode1SpritePose *target;
    loom_s16 index;
#if !defined(LOOM_TARGET_PVSNESLIB)
    LoomStatus status;
#endif

    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
#if !defined(LOOM_TARGET_PVSNESLIB)
    status = loom_mode1_validate_pose(pose);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
#endif
    index = loom_mode1_sprite_index(slot);
    if (index < 0 || part == 0u ||
        part > loom_mode1_state.sprite_part_count[(loom_u8)index] ||
        (loom_u16)(index + part) >= loom_mode1_state.scene->sprite_count) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    target = &loom_mode1_state.sprite_pose[(loom_u8)(index + part)];
    loom_mode1_pose_dirty[(loom_u8)(index + part)] = LOOM_TRUE;
    target->pivot_x = pose->pivot_x;
    target->pivot_y = pose->pivot_y;
    target->tile_index = pose->tile_index;
    target->palette = pose->palette;
    target->size = pose->size;
    target->width = pose->width;
    target->height = pose->height;
    target->flags = pose->flags;
    return LOOM_STATUS_OK;
}

LoomStatus loom_mode1_set_sprite_visible(loom_u8 slot, loom_u8 visible)
{
    loom_s16 index;

    if (loom_mode1_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    if (visible > LOOM_TRUE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    index = loom_mode1_sprite_index(slot);
    if (index < 0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    {
        /* Hiding an actor hides the whole of it. */
        loom_u8 remaining;
        loom_u8 cursor;

        cursor = (loom_u8)index;
        remaining = loom_mode1_state.sprite_part_count[cursor];
        loom_mode1_state.sprite_visible[cursor] = visible;
        while (remaining != 0u &&
               (loom_u16)(cursor + 1u) < loom_mode1_state.scene->sprite_count) {
            ++cursor;
            --remaining;
            loom_mode1_state.sprite_visible[cursor] = visible;
        }
    }
    return LOOM_STATUS_OK;
}

const loom_u8 *loom_mode1_visibility_table(void)
{
    return loom_mode1_state.sprite_visible;
}

loom_s16 loom_mode1_slot_sprite_index(loom_u8 slot)
{
    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return (loom_s16)-1;
    }
    return loom_mode1_sprite_index(slot);
}

loom_u8 loom_mode1_sprite_visible(loom_u8 slot)
{
    loom_s16 index;

    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_FALSE;
    }
    index = loom_mode1_sprite_index(slot);
    if (index < 0) {
        return LOOM_FALSE;
    }
    return loom_mode1_state.sprite_visible[(loom_u8)index];
}

LoomStatus loom_mode1_sprite_pose(loom_u8 slot,
                                  LoomMode1SpritePose *pose)
{
    loom_s16 index;

    if (pose == (LoomMode1SpritePose *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    index = loom_mode1_sprite_index(slot);
    if (index < 0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    pose->pivot_x = loom_mode1_state.sprite_pose[(loom_u8)index].pivot_x;
    pose->pivot_y = loom_mode1_state.sprite_pose[(loom_u8)index].pivot_y;
    pose->tile_index =
        loom_mode1_state.sprite_pose[(loom_u8)index].tile_index;
    pose->palette = loom_mode1_state.sprite_pose[(loom_u8)index].palette;
    pose->size = loom_mode1_state.sprite_pose[(loom_u8)index].size;
    pose->width = loom_mode1_state.sprite_pose[(loom_u8)index].width;
    pose->height = loom_mode1_state.sprite_pose[(loom_u8)index].height;
    pose->flags = loom_mode1_state.sprite_pose[(loom_u8)index].flags;
    return LOOM_STATUS_OK;
}

LoomStatus loom_mode1_camera_bounds(loom_s16 *min_x, loom_s16 *min_y,
                                    loom_s16 *max_x, loom_s16 *max_y)
{
    if (min_x == (loom_s16 *)0 || min_y == (loom_s16 *)0 ||
        max_x == (loom_s16 *)0 || max_y == (loom_s16 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    *min_x = loom_mode1_state.scene->camera_min_x;
    *min_y = loom_mode1_state.scene->camera_min_y;
    *max_x = loom_mode1_state.scene->camera_max_x;
    *max_y = loom_mode1_state.scene->camera_max_y;
    return LOOM_STATUS_OK;
}

loom_s16 loom_mode1_camera_x(void)
{
    return loom_mode1_state.camera_x;
}

loom_s16 loom_mode1_camera_y(void)
{
    return loom_mode1_state.camera_y;
}

void loom_mode1_debug_snapshot(LoomMode1DebugSnapshot *snapshot)
{
    snapshot->camera_x = loom_mode1_state.camera_x;
    snapshot->camera_y = loom_mode1_state.camera_y;
    snapshot->ready = loom_mode1_state.ready;
    snapshot->brightness = loom_mode1_state.brightness;
    snapshot->raster_enabled = loom_mode1_state.raster_enabled;
    snapshot->tile_animation_frame = loom_mode1_state.animation_frame[0];
    snapshot->palette_cycle_step = loom_mode1_state.cycle_step[0];
    snapshot->reserved = 0u;
}

loom_u8 loom_mode1_brightness(void)
{
    return loom_mode1_state.brightness;
}

loom_u8 loom_mode1_ready(void)
{
    return loom_mode1_state.ready;
}

loom_u8 loom_mode1_raster_enabled(void)
{
    return loom_mode1_state.raster_enabled;
}
