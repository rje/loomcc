/* The C that oam.asm's loom_pvs_mode1_scroll replaced (Loom cbd8c0c^:
 * runtime/src/mode1.c:1264-1331, loom_mode1_build_frame's scroll block, and
 * loom_mode1_scaled with its shift table, mode1.c:919-951), cut out as a
 * function with the assembly routine's name and interface. The block's
 * state (the scene's layers, the camera, the cache) arrives as arguments.
 *
 * Two changes the assembly's contract makes: a drifting layer returns 0xffff
 * at that layer (the C then runs the whole block itself, auto-scroll
 * included), so the auto-scroll advance and the layer_auto_x/y terms -- zero
 * for a scene with no drifting layer -- are left out; and the cache is marked
 * valid unconditionally, since only a drift-free pass reaches the end. */
#include "loom_types.h"

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

loom_u16 loom_pvs_mode1_scroll(LoomDisplayState *display,
                               LoomMode1ScrollCache *cache,
                               const LoomMode1Layer *layers,
                               loom_u16 layer_count, loom_s16 camera_x,
                               loom_s16 camera_y)
{
    loom_u8 layer;
    loom_u8 has_bg2;
    loom_u8 has_bg3;

    if (cache->valid != LOOM_FALSE &&
        cache->camera_x == camera_x &&
        cache->camera_y == camera_y) {
        /* Written out: an indexed store through a pointer is a multiply
         * and an add per element on 816-tcc, about 340 instructions for
         * this copy as a loop. */
        display->bg_scroll_x[0] = cache->x[0];
        display->bg_scroll_x[1] = cache->x[1];
        display->bg_scroll_x[2] = cache->x[2];
        display->bg_scroll_x[3] = cache->x[3];
        display->bg_scroll_y[0] = cache->y[0];
        display->bg_scroll_y[1] = cache->y[1];
        display->bg_scroll_y[2] = cache->y[2];
        display->bg_scroll_y[3] = cache->y[3];
        has_bg2 = cache->has_bg2;
        has_bg3 = cache->has_bg3;
    } else {
    display->bg_scroll_x[0] = camera_x;
    display->bg_scroll_y[0] = camera_y;
    has_bg2 = LOOM_FALSE;
    has_bg3 = LOOM_FALSE;
    for (layer = 0u; layer < layer_count; ++layer) {
        const LoomMode1Layer *record;
        loom_u8 background;

        record = &layers[layer];
        background = record->background;
        if (record->auto_scroll_x != 0 || record->auto_scroll_y != 0) {
            return 0xffffu;
        }
        display->bg_scroll_x[background] = (loom_s16)(
            loom_mode1_scaled(camera_x,
                              record->scroll_x_numerator,
                              record->scroll_x_denominator));
        display->bg_scroll_y[background] = (loom_s16)(
            loom_mode1_scaled(camera_y,
                              record->scroll_y_numerator,
                              record->scroll_y_denominator));
        if (background == 1u) {
            has_bg2 = LOOM_TRUE;
        } else if (background == 2u) {
            has_bg3 = LOOM_TRUE;
        }
    }
    cache->x[0] = display->bg_scroll_x[0];
    cache->x[1] = display->bg_scroll_x[1];
    cache->x[2] = display->bg_scroll_x[2];
    cache->x[3] = display->bg_scroll_x[3];
    cache->y[0] = display->bg_scroll_y[0];
    cache->y[1] = display->bg_scroll_y[1];
    cache->y[2] = display->bg_scroll_y[2];
    cache->y[3] = display->bg_scroll_y[3];
    cache->camera_x = camera_x;
    cache->camera_y = camera_y;
    cache->has_bg2 = has_bg2;
    cache->has_bg3 = has_bg3;
    cache->valid = LOOM_TRUE;
    }
    return (loom_u16)((loom_u16)has_bg2 | ((loom_u16)has_bg3 << 1));
}
