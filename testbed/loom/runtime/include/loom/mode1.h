#ifndef LOOM_MODE1_H
#define LOOM_MODE1_H

#include <loom/frame.h>

#define LOOM_MODE1_VIEW_WIDTH ((loom_u16)256u)
#define LOOM_MODE1_VIEW_HEIGHT ((loom_u16)224u)
#define LOOM_MODE1_DMA_CHUNK_BYTES ((loom_u16)32u)
/* The planner's activation pacing, which it budgets VBlank against. The
 * runtime's loads run under forced blank and fill each commit instead:
 * everything the frame capacity leaves (loom/frame.h). */
#define LOOM_MODE1_LOAD_JOBS_PER_COMMIT ((loom_u8)12u)
#define LOOM_MODE1_LOAD_BYTES_PER_COMMIT ((loom_u16)384u)
/* The fixed BG1 map: 64x32 tiles at VRAM byte 0x4000. */
#define LOOM_MODE1_BG1_MAP_VRAM_BYTE ((loom_u16)0x4000u)

typedef struct LoomMode1LoadSegment {
    LoomAssetHandle source_handle;
    loom_u16 source_offset;
    loom_u16 destination_offset;
    loom_u16 byte_count;
    loom_u8 destination_kind;
    loom_u8 reserved;
} LoomMode1LoadSegment;

typedef struct LoomMode1Sprite {
    loom_s16 world_x;
    loom_s16 world_y;
    loom_s16 pivot_x;
    loom_s16 pivot_y;
    loom_u16 tile_index;
    loom_u8 slot;
    loom_u8 palette;
    loom_u8 priority;
    loom_u8 size;
    loom_u8 flags;
    loom_u8 width;
    loom_u8 height;
    /* Slots after this one holding the rest of a metasprite. They carry their
     * own pivots and poses; moving or hiding this sprite moves and hides them
     * with it, which is how one actor becomes several hardware sprites without
     * the OAM builder knowing. */
    loom_u8 part_count;
    loom_u8 reserved;
} LoomMode1Sprite;

/* The console's OAM path in runtime/backends/pvsneslib/src/oam.asm walks the
 * sprite and pose arrays by hand, so it needs these two sizes spelled out.
 * A field added here without the assembly's stride following makes every
 * record after the first read the previous one's tail, which is how the
 * Lantern Road cartridge came to sit on its title screen for five commits.
 * `oam_assembly_strides_match_the_runtime_structs` compares these numbers to
 * the assembly's own; the assertions below tie them to the structs. */
#define LOOM_MODE1_SPRITE_BYTES 20
LOOM_STATIC_ASSERT(loom_mode1_sprite_matches_its_stride,
                   sizeof(LoomMode1Sprite) == LOOM_MODE1_SPRITE_BYTES);

typedef struct LoomMode1SpritePose {
    loom_s16 pivot_x;
    loom_s16 pivot_y;
    loom_u16 tile_index;
    loom_u8 palette;
    loom_u8 size;
    loom_u8 width;
    loom_u8 height;
    /* OAM flips this pose adds to the authored sprite's own flags, so a
     * mirrored animation can face left without a second clip. */
    loom_u8 flags;
} LoomMode1SpritePose;

#define LOOM_MODE1_SPRITE_POSE_BYTES 12
LOOM_STATIC_ASSERT(loom_mode1_sprite_pose_matches_its_stride,
                   sizeof(LoomMode1SpritePose) == LOOM_MODE1_SPRITE_POSE_BYTES);

/* Everything the sprite build reads, so a target can build the frame's
 * sprites in one pass (the pvsneslib console does so in assembly). */
typedef struct LoomMode1SpriteBatch {
    const LoomMode1Sprite *sprites;
    const LoomMode1SpritePose *poses;
    const loom_u8 *visible;
    const loom_s16 *world_x;
    const loom_s16 *world_y;
    loom_s16 camera_x;
    loom_s16 camera_y;
    loom_u8 count;
    loom_u8 reserved;
} LoomMode1SpriteBatch;

/* A resident tile layer: its background slot and how it follows the camera.
 * Scroll is camera * numerator / denominator plus an auto-scroll drift in
 * 1/256 pixel per frame. */
typedef struct LoomMode1Layer {
    loom_u8 background;
    loom_u8 above_sprites;
    loom_u8 scroll_x_numerator;
    loom_u8 scroll_x_denominator;
    loom_u8 scroll_y_numerator;
    loom_u8 scroll_y_denominator;
    loom_s16 auto_scroll_x;
    loom_s16 auto_scroll_y;
} LoomMode1Layer;

LOOM_STATIC_ASSERT(loom_mode1_layer_is_ten_bytes,
                   sizeof(LoomMode1Layer) == 10u);

/* BG1, BG2 and a BG3 gameplay layer; the cartridge UI's background is never
 * among a scene's layers. */
#define LOOM_MODE1_LAYER_CAPACITY ((loom_u8)3u)
#define LOOM_MODE1_SCROLL_TERM_MAX ((loom_u8)8u)

/* A scene larger than the hardware map streams its BG1 layer. The grid holds
 * one metatile id per cell in row order (LOOM_MODE1_STREAM_BLANK for empty);
 * the table holds four map words per metatile (top-left, top-right,
 * bottom-left, bottom-right). Activation loads the window at
 * initial_column/initial_row; each frame then expands at most one column and
 * two rows into the stream block and DMAs them at the camera's edge. */
#define LOOM_MODE1_STREAM_BLANK ((loom_u8)255u)
#define LOOM_MODE1_STREAM_METATILE_MAX ((loom_u8)255u)
#define LOOM_MODE1_STREAM_SPAN_MAX ((loom_u16)256u)
#define LOOM_MODE1_STREAM_CELLS_MAX ((loom_u16)16384u)
#define LOOM_MODE1_STREAM_WINDOW_COLUMNS ((loom_u16)32u)
#define LOOM_MODE1_STREAM_WINDOW_ROWS ((loom_u16)16u)
#define LOOM_MODE1_STREAM_LOOK_BEHIND ((loom_u16)8u)
#define LOOM_MODE1_STREAM_BLOCK_HANDLE ((LoomWramBlockHandle)0u)
#define LOOM_MODE1_STREAM_COLUMN_WORDS ((loom_u16)64u)
#define LOOM_MODE1_STREAM_ROW_WORDS ((loom_u16)128u)
#define LOOM_MODE1_STREAM_BUFFER_WORDS ((loom_u16)320u)
#define LOOM_MODE1_STREAM_ROWS_PER_FRAME ((loom_u8)2u)

typedef struct LoomMode1Stream {
    loom_u16 width_metatiles;
    loom_u16 height_metatiles;
    loom_u16 initial_column;
    loom_u16 initial_row;
    loom_u16 blank_word;
    loom_u8 metatile_count;
    loom_u8 reserved;
    const loom_u8 *grid;
    const loom_u16 *table;
} LoomMode1Stream;

/* One DMA of a tile animation frame: a run of contiguous hardware tiles,
 * `byte_count` bytes at `source_offset` within a frame, landing at
 * `destination_offset` in VRAM. */
typedef struct LoomMode1TileRun {
    loom_u16 source_offset;
    loom_u16 destination_offset;
    loom_u16 byte_count;
    loom_u16 reserved;
} LoomMode1TileRun;

/* A tile animation: the first frame's hardware tiles redrawn from `frames`
 * (a ROM asset holding `frame_count` frames of `frame_stride` bytes) on the
 * cadence of `durations`, in logical ticks per frame. */
typedef struct LoomMode1TileAnimation {
    LoomAssetHandle frames;
    loom_u16 frame_stride;
    loom_u8 frame_count;
    loom_u8 run_count;
    const loom_u16 *durations;
    const LoomMode1TileRun *runs;
} LoomMode1TileAnimation;

/* A palette cycle: `step_count` precomputed rotations of `byte_count` bytes
 * in `steps`, one written to CGRAM `destination_offset` every
 * `period_ticks` logical ticks. */
typedef struct LoomMode1PaletteCycle {
    LoomAssetHandle steps;
    loom_u16 destination_offset;
    loom_u16 byte_count;
    loom_u16 period_ticks;
    loom_u8 step_count;
    loom_u8 reserved;
} LoomMode1PaletteCycle;

/* A raster program the runtime drives from its own WRAM table. A scroll
 * band program writes one horizontal scroll per band from the camera each
 * tick; a wave carries a phase in the binding's state and the target adapter
 * points the HDMA table into the generated ROM sine data. The gradients play
 * from ROM; the fixed one still asks for BG1's colour math. */
#define LOOM_MODE1_RASTER_NONE ((loom_u8)0u)
#define LOOM_MODE1_RASTER_FIXED_COLOR ((loom_u8)1u)
#define LOOM_MODE1_RASTER_BACKDROP_GRADIENT ((loom_u8)2u)
#define LOOM_MODE1_RASTER_SCROLL_BANDS ((loom_u8)3u)
#define LOOM_MODE1_RASTER_WAVE ((loom_u8)4u)
#define LOOM_MODE1_RASTER_BANDS_MAX ((loom_u8)8u)
/* Three bytes a band plus the terminator, double-buffered so the frame the
 * hardware is reading is never the one being written. */
#define LOOM_MODE1_RASTER_TABLE_BYTES ((loom_u8)32u)

typedef struct LoomMode1RasterParams {
    loom_u8 kind;
    /* The background the program moves: 0 for BG1. */
    loom_u8 layer;
    loom_u8 band_count;
    loom_u8 wave_amplitude;
    loom_u8 wave_wavelength;
    loom_s8 wave_speed;
    loom_u8 reserved0;
    loom_u8 reserved1;
    loom_u8 band_lines[LOOM_MODE1_RASTER_BANDS_MAX];
    loom_u8 band_numerators[LOOM_MODE1_RASTER_BANDS_MAX];
    loom_u8 band_denominators[LOOM_MODE1_RASTER_BANDS_MAX];
} LoomMode1RasterParams;

LOOM_STATIC_ASSERT(loom_mode1_raster_params_is_thirty_two_bytes,
                   sizeof(LoomMode1RasterParams) == 32u);

/* The scroll band tables, one per buffer; the target adapter reads the one
 * the binding's state names. */
extern loom_u8 loom_mode1_raster_tables[2][LOOM_MODE1_RASTER_TABLE_BYTES];

/* How the console blends one layer of a scene. */
#define LOOM_MODE1_COLOR_MATH_NORMAL ((loom_u8)0u)
/* The layer moves to the sub screen; every main layer adds it at half. */
#define LOOM_MODE1_COLOR_MATH_TRANSLUCENT ((loom_u8)1u)
/* A grey of `amount` is subtracted from the layer alone. */
#define LOOM_MODE1_COLOR_MATH_DARKEN ((loom_u8)2u)

#define LOOM_MODE1_TILE_ANIMATIONS_MAX ((loom_u8)8u)
#define LOOM_MODE1_PALETTE_CYCLES_MAX ((loom_u8)4u)
/* Per tick the animations stage at most one tile animation (two runs) and
 * one palette cycle step, the headroom a streamed room with a UI leaves. */
#define LOOM_MODE1_TILE_RUNS_MAX ((loom_u8)2u)

typedef struct LoomMode1Scene {
    loom_u16 pixel_width;
    loom_u16 pixel_height;
    loom_s16 camera_min_x;
    loom_s16 camera_min_y;
    loom_s16 camera_max_x;
    loom_s16 camera_max_y;
    loom_s16 initial_camera_x;
    loom_s16 initial_camera_y;
    loom_u16 backdrop_color;
    LoomRasterBinding raster;
    loom_u8 obj_size_pair;
    loom_u8 load_segment_count;
    loom_u8 sprite_count;
    loom_u8 reserved;
    const LoomMode1LoadSegment *load_segments;
    const LoomMode1Sprite *sprites;
    /* Zero layers keeps the v0 behaviour: BG1 follows the camera exactly. */
    loom_u8 layer_count;
    loom_u8 layer_reserved;
    const LoomMode1Layer *layers;
    /* Null for scenes that fit the hardware map. */
    const LoomMode1Stream *stream;
    /* Trailing so older literals (which stop at `stream`) still zero-fill:
     * a scene with no animation carries counts of zero and null tables. */
    loom_u8 tile_animation_count;
    loom_u8 palette_cycle_count;
    const LoomMode1TileAnimation *tile_animations;
    const LoomMode1PaletteCycle *palette_cycles;
    /* What the raster program owes the runtime; null with no program. */
    const LoomMode1RasterParams *raster_params;
    /* LOOM_LAYER_* mask of the one blended layer, its mode and amount. */
    loom_u8 color_math_layers;
    loom_u8 color_math_mode;
    loom_u8 color_math_amount;
    loom_u8 color_math_reserved;
} LoomMode1Scene;

/* Defined by generated mode1_data.c for the selected initial scene. */
extern const loom_u8 loom_generated_mode1_enabled;
extern const LoomMode1Scene loom_generated_mode1_initial_scene;

LoomStatus loom_mode1_initialize(void);
LoomStatus loom_mode1_activate_scene(const LoomMode1Scene *scene);
LoomStatus loom_mode1_build_frame(const LoomFrameBoundary *boundary);
LoomStatus loom_mode1_set_brightness(loom_u8 brightness);
LoomStatus loom_mode1_set_raster_enabled(loom_u8 enabled);
/* The wave's phase in scanlines, for tests and the witness. */
loom_u16 loom_mode1_wave_phase(void);
LoomStatus loom_mode1_set_camera(loom_s16 x, loom_s16 y);
LoomStatus loom_mode1_set_sprite_position(loom_u8 slot,
                                          loom_s16 world_x,
                                          loom_s16 world_y);
LoomStatus loom_mode1_sprite_position(loom_u8 slot,
                                      loom_s16 *world_x,
                                      loom_s16 *world_y);
LoomStatus loom_mode1_set_sprite_pose(loom_u8 slot,
                                      const LoomMode1SpritePose *pose);
/* Pose part `part` (1 through the sprite's part_count) of the metasprite
 * whose own sprite is `slot`: the sprite that many entries after it. */
LoomStatus loom_mode1_set_sprite_part_pose(loom_u8 slot,
                                           loom_u8 part,
                                           const LoomMode1SpritePose *pose);
LoomStatus loom_mode1_set_sprite_visible(loom_u8 slot, loom_u8 visible);
/* False for a hidden sprite and for a slot the scene does not draw, which is
 * how a gated actor stops taking part in the world. */
loom_u8 loom_mode1_sprite_visible(loom_u8 slot);
/* The visibility byte per sprite *index* (not slot), for modules that walk
 * many sprites a tick: one pointer read instead of a call per sprite. The
 * table lives for the life of the program; indices come from
 * loom_mode1_slot_sprite_index at activation. */
const loom_u8 *loom_mode1_visibility_table(void);
/* The sprite index behind an OAM slot, or -1 when the scene has none. */
loom_s16 loom_mode1_slot_sprite_index(loom_u8 slot);
LoomStatus loom_mode1_sprite_pose(loom_u8 slot,
                                  LoomMode1SpritePose *pose);
/* The active scene's camera clamp, for follow rules that page the view. */
LoomStatus loom_mode1_camera_bounds(loom_s16 *min_x, loom_s16 *min_y,
                                    loom_s16 *max_x, loom_s16 *max_y);
loom_s16 loom_mode1_camera_x(void);
loom_s16 loom_mode1_camera_y(void);
/* The WRAM block map streaming DMAs from (LOOM_MODE1_STREAM_BLOCK_HANDLE). */
loom_u8 *loom_mode1_stream_block(loom_u16 *bytes);
/* The leftmost column and topmost row currently loaded in the hardware map. */
loom_u16 loom_mode1_stream_column(void);
loom_u16 loom_mode1_stream_row(void);
/* The frame a tile animation shows and the step a palette cycle is on, by
 * the scene's own index order; zero for an index the scene does not use. */
loom_u8 loom_mode1_tile_animation_frame(loom_u8 index);
loom_u8 loom_mode1_palette_cycle_step(loom_u8 index);
typedef struct LoomMode1DebugSnapshot {
    loom_s16 camera_x;
    loom_s16 camera_y;
    loom_u8 ready;
    loom_u8 brightness;
    loom_u8 raster_enabled;
    /* The first tile animation's frame and the first palette cycle's step. */
    loom_u8 tile_animation_frame;
    loom_u8 palette_cycle_step;
    loom_u8 reserved;
} LoomMode1DebugSnapshot;
void loom_mode1_debug_snapshot(LoomMode1DebugSnapshot *snapshot);
extern loom_u8 loom_mode1_debug_epoch;
loom_u8 loom_mode1_brightness(void);
loom_u8 loom_mode1_ready(void);
loom_u8 loom_mode1_raster_enabled(void);

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
/* body.asm's actor pass moves sprites straight in these tables; they are
 * bound at every scene activation. */
void loom_pvs_mode1_bind(loom_s16 *world_x, loom_s16 *world_y,
                         loom_u8 *part_count, loom_u16 sprite_count,
                         const loom_u8 *slot_index, loom_s16 *camera_xy,
                         loom_s16 camera_min_x, loom_s16 camera_min_y,
                         loom_s16 camera_max_x, loom_s16 camera_max_y);
#endif

#endif
