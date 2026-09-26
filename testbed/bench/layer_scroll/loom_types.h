/* The Loom types layer_scroll needs, copied from Loom cbd8c0c^
 * (runtime/include/loom/types.h, video.h, mode1.h and the scroll cache in
 * runtime/src/mode1.c). The assembly reads all three structs by offset. */
#ifndef LAYER_SCROLL_LOOM_TYPES_H
#define LAYER_SCROLL_LOOM_TYPES_H

typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)

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

#define LOOM_MODE1_SCROLL_TERM_MAX ((loom_u8)8u)

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

/* The assembly's offsets (oam.asm header comment). */
typedef char layer_scroll_layer_is_10[(sizeof(LoomMode1Layer) == 10u) ? 1 : -1];
typedef char layer_scroll_cache_is_24[(sizeof(LoomMode1ScrollCache) == 24u) ? 1 : -1];

loom_u16 loom_pvs_mode1_scroll(LoomDisplayState *display,
                               LoomMode1ScrollCache *cache,
                               const LoomMode1Layer *layers,
                               loom_u16 layer_count, loom_s16 camera_x,
                               loom_s16 camera_y);

#endif
