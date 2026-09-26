/* The Loom types display_block needs, copied from Loom 72f1cd4^
 * (runtime/include/loom/types.h, raster.h, video.h, mode1.h). The tables the
 * scene points at are left incomplete: the unit never follows them, but
 * their pointers fix the layout the assembly reads by offset. */
#ifndef DISPLAY_BLOCK_LOOM_TYPES_H
#define DISPLAY_BLOCK_LOOM_TYPES_H

typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)
#define LOOM_INVALID_HANDLE ((loom_u16)0xffffu)

typedef loom_u16 LoomRasterProgramHandle;
typedef loom_u16 LoomRasterStateHandle;

typedef struct LoomRasterBinding {
    LoomRasterProgramHandle program;
    LoomRasterStateHandle state;
} LoomRasterBinding;

#define LOOM_RASTER_PROGRAM_NONE \
    ((LoomRasterProgramHandle)LOOM_INVALID_HANDLE)
#define LOOM_RASTER_STATE_NONE ((LoomRasterStateHandle)LOOM_INVALID_HANDLE)

#define LOOM_LAYER_BG1 ((loom_u8)0x01u)
#define LOOM_LAYER_BG2 ((loom_u8)0x02u)
#define LOOM_LAYER_BG3 ((loom_u8)0x04u)
#define LOOM_LAYER_OBJ ((loom_u8)0x10u)
#define LOOM_DISPLAY_FORCED_BLANK ((loom_u8)0x01u)

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

typedef struct LoomMode1LoadSegment LoomMode1LoadSegment;
typedef struct LoomMode1Sprite LoomMode1Sprite;
typedef struct LoomMode1Layer LoomMode1Layer;
typedef struct LoomMode1Stream LoomMode1Stream;
typedef struct LoomMode1TileAnimation LoomMode1TileAnimation;
typedef struct LoomMode1PaletteCycle LoomMode1PaletteCycle;
typedef struct LoomMode1RasterParams LoomMode1RasterParams;

#define LOOM_MODE1_COLOR_MATH_NORMAL ((loom_u8)0u)
#define LOOM_MODE1_COLOR_MATH_TRANSLUCENT ((loom_u8)1u)
#define LOOM_MODE1_COLOR_MATH_DARKEN ((loom_u8)2u)

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
    loom_u8 layer_count;
    loom_u8 layer_reserved;
    const LoomMode1Layer *layers;
    const LoomMode1Stream *stream;
    loom_u8 tile_animation_count;
    loom_u8 palette_cycle_count;
    const LoomMode1TileAnimation *tile_animations;
    const LoomMode1PaletteCycle *palette_cycles;
    const LoomMode1RasterParams *raster_params;
    loom_u8 color_math_layers;
    loom_u8 color_math_mode;
    loom_u8 color_math_amount;
    loom_u8 color_math_reserved;
} LoomMode1Scene;

#ifdef __65816__
/* The assembly's offsets (oam.asm; mode1.c asserts the same at 72f1cd4). */
typedef char display_block_scene_is_68[(sizeof(LoomMode1Scene) == 68u) ? 1 : -1];
typedef char display_block_display_is_31[(sizeof(LoomDisplayState) == 31u ||
                                           sizeof(LoomDisplayState) == 32u) ? 1 : -1];
#endif

loom_u16 loom_pvs_mode1_display(LoomDisplayState *display,
                                LoomRasterBinding *raster,
                                const LoomMode1Scene *scene, loom_u16 bits);

#endif
