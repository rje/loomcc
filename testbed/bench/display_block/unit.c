/* The C that oam.asm's loom_pvs_mode1_display replaced (Loom 72f1cd4^:
 * runtime/src/mode1.c:1377-1402, loom_mode1_build_frame's display block
 * after the scroll), cut out as a function with the assembly routine's name
 * and interface. The mode1 state it read (ready, raster_enabled, brightness)
 * and the scroll block's has_bg2/has_bg3 arrive packed in `bits` as the
 * assembly takes them: has_bg2 1, has_bg3 2, ready 4, raster_enabled 8,
 * brightness << 8. The two calls that stay C (loom_mode1_apply_color_math,
 * which acts only for a non-NORMAL colour-math mode, and
 * loom_mode1_drive_raster) are reported in the result as the assembly
 * reports them: lit 1, colour math due 2, raster drive due 4. */
#include "loom_types.h"

loom_u16 loom_pvs_mode1_display(LoomDisplayState *display,
                                LoomRasterBinding *raster,
                                const LoomMode1Scene *scene, loom_u16 bits)
{
    loom_u8 has_bg2;
    loom_u8 has_bg3;
    loom_u8 ready;
    loom_u8 raster_enabled;
    loom_u8 brightness;
    loom_u8 lit;
    loom_u16 todo;

    has_bg2 = (loom_u8)(bits & 1u);
    has_bg3 = (loom_u8)((bits >> 1) & 1u);
    ready = (loom_u8)((bits & 4u) != 0u);
    raster_enabled = (loom_u8)((bits & 8u) != 0u);
    brightness = (loom_u8)(bits >> 8);

    lit = (loom_u8)(ready != LOOM_FALSE &&
                    brightness != 0u);
    display->backdrop_color = scene->backdrop_color;
    display->brightness = ready != LOOM_FALSE
                              ? brightness
                              : 0u;
    display->main_layers =
        lit != LOOM_FALSE
            ? (loom_u8)(LOOM_LAYER_BG1 | LOOM_LAYER_OBJ |
                        (has_bg2 != LOOM_FALSE ? LOOM_LAYER_BG2 : 0u) |
                        (has_bg3 != LOOM_FALSE ? LOOM_LAYER_BG3 : 0u))
            : 0u;
    display->obj_size_pair = scene->obj_size_pair;
    todo = lit;
    if (lit != LOOM_FALSE) {
        /* loom_mode1_apply_color_math(display) */
        if (scene->color_math_mode != LOOM_MODE1_COLOR_MATH_NORMAL) {
            todo |= 2u;
        }
    }
    *raster = scene->raster;
    if (raster_enabled == LOOM_FALSE ||
        ready == LOOM_FALSE) {
        raster->program = LOOM_RASTER_PROGRAM_NONE;
        raster->state = LOOM_RASTER_STATE_NONE;
    }
    if (raster->program != LOOM_RASTER_PROGRAM_NONE) {
        /* loom_mode1_drive_raster(display, raster) */
        todo |= 4u;
    }
    display->flags = lit != LOOM_FALSE ? 0u : LOOM_DISPLAY_FORCED_BLANK;
    return todo;
}
