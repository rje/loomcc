/* display_block: loom_mode1_build_frame's display block after the scroll,
 * over three scenes (a parallax room with a scroll-band raster program and
 * translucent BG3, a dark cave with darken colour math and no raster, a
 * plain room) and the states a tick sees: lit with each BG2/BG3 mix, a
 * fade (brightness 0 while ready), a scene still loading, raster disabled,
 * full brightness. Each call writes its own display and raster binding so
 * every result survives to the check. */
#include "bench.h"
#include "loom_types.h"

#define CALLS 12

static LoomMode1Scene scenes[3];
static LoomDisplayState displays[CALLS];
static LoomRasterBinding rasters[CALLS];
static loom_u16 results[CALLS];

#define BITS(bg2, bg3, ready, raster_on, bright) \
    ((loom_u16)((bg2) | ((bg3) << 1) | ((ready) << 2) | ((raster_on) << 3) | ((bright) << 8)))

void bench_setup(void)
{
    unsigned short i, j;
    unsigned char *bytes;

    for (i = 0; i < 3; i++) {
        bytes = (unsigned char *)&scenes[i];
        for (j = 0; j < sizeof(LoomMode1Scene); j++)
            bytes[j] = (unsigned char)(0x31u + j * 7u + i);
    }
    scenes[0].backdrop_color = 0x7c1fu;
    scenes[0].raster.program = 2u;
    scenes[0].raster.state = 0u;
    scenes[0].obj_size_pair = 1u;
    scenes[0].color_math_layers = LOOM_LAYER_BG3;
    scenes[0].color_math_mode = LOOM_MODE1_COLOR_MATH_TRANSLUCENT;
    scenes[1].backdrop_color = 0x0421u;
    scenes[1].raster.program = LOOM_RASTER_PROGRAM_NONE;
    scenes[1].raster.state = LOOM_RASTER_STATE_NONE;
    scenes[1].obj_size_pair = 3u;
    scenes[1].color_math_layers = LOOM_LAYER_BG2;
    scenes[1].color_math_mode = LOOM_MODE1_COLOR_MATH_DARKEN;
    scenes[2].backdrop_color = 0x2d6bu;
    scenes[2].raster.program = 0u;
    scenes[2].raster.state = 7u;
    scenes[2].obj_size_pair = 0u;
    scenes[2].color_math_layers = 0u;
    scenes[2].color_math_mode = LOOM_MODE1_COLOR_MATH_NORMAL;

    /* The open commit's default display and a stale binding. */
    for (i = 0; i < CALLS; i++) {
        bytes = (unsigned char *)&displays[i];
        for (j = 0; j < sizeof(LoomDisplayState); j++)
            bytes[j] = (unsigned char)(0x80u + j + i * 3u);
        rasters[i].program = (loom_u16)(0x5a00u + i);
        rasters[i].state = (loom_u16)(0xa500u + i);
    }
}

void bench_run(void)
{
    results[0] = loom_pvs_mode1_display(&displays[0], &rasters[0], &scenes[0], BITS(1, 1, 1, 1, 15));
    results[1] = loom_pvs_mode1_display(&displays[1], &rasters[1], &scenes[0], BITS(1, 0, 1, 1, 8));
    results[2] = loom_pvs_mode1_display(&displays[2], &rasters[2], &scenes[0], BITS(1, 1, 1, 0, 15));
    results[3] = loom_pvs_mode1_display(&displays[3], &rasters[3], &scenes[0], BITS(1, 1, 0, 1, 15));
    results[4] = loom_pvs_mode1_display(&displays[4], &rasters[4], &scenes[0], BITS(0, 1, 1, 1, 0));
    results[5] = loom_pvs_mode1_display(&displays[5], &rasters[5], &scenes[1], BITS(1, 0, 1, 1, 15));
    results[6] = loom_pvs_mode1_display(&displays[6], &rasters[6], &scenes[1], BITS(0, 0, 1, 0, 3));
    results[7] = loom_pvs_mode1_display(&displays[7], &rasters[7], &scenes[1], BITS(0, 1, 0, 0, 0));
    results[8] = loom_pvs_mode1_display(&displays[8], &rasters[8], &scenes[2], BITS(0, 0, 1, 1, 15));
    results[9] = loom_pvs_mode1_display(&displays[9], &rasters[9], &scenes[2], BITS(1, 1, 1, 1, 1));
    results[10] = loom_pvs_mode1_display(&displays[10], &rasters[10], &scenes[2], BITS(1, 0, 0, 1, 12));
    results[11] = loom_pvs_mode1_display(&displays[11], &rasters[11], &scenes[2], BITS(0, 1, 1, 1, 0));
}

static unsigned short fold(unsigned short h, const unsigned char *bytes, unsigned short count)
{
    unsigned short i;
    for (i = 0; i < count; i++)
        h = (unsigned short)(((unsigned short)(h << 5) | (unsigned short)(h >> 11)) ^ bytes[i]);
    return h;
}

void bench_check(void)
{
    unsigned short i, h;
    for (i = 0; i < CALLS; i++)
        BENCH_OUT(results[i]);
    for (i = 0; i < CALLS; i++) {
        h = fold(0x1234u, (const unsigned char *)&displays[i], 31u);
        BENCH_OUT(fold(h, (const unsigned char *)&rasters[i], 4u));
    }
    BENCH_OUT(displays[0].main_layers | (displays[0].brightness << 8));
    BENCH_OUT(displays[5].obj_size_pair | (displays[5].flags << 8));
    BENCH_OUT(rasters[0].program);
    BENCH_OUT(rasters[9].state);
}
