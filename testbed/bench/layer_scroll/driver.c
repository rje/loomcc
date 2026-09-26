/* layer_scroll: loom_mode1_build_frame's scroll block over a three-layer
 * parallax scene (BG1 1/1, BG2 at 1/2 x 1/4 y, BG3 at 3/8 x 2/3 y) walked
 * by a camera the way a tick does: moving (the full compute), standing
 * still (the cache copy), negative (sign handling), denominators that are
 * not powers of two (3 and 7: the divide), a two-layer scene without BG3, a
 * scene with no layers, and a drifting scene the block leaves to C (0xffff).
 * Each call writes its own display so every result survives to the check.
 * Eight calls keep the tcc variant inside one frame. */
#include "bench.h"
#include "loom_types.h"

#define CALLS 8

static const LoomMode1Layer parallax[3] = {
    {0u, 0u, 1u, 1u, 1u, 1u, 0, 0},
    {1u, 0u, 1u, 2u, 1u, 4u, 0, 0},
    {2u, 1u, 3u, 8u, 2u, 3u, 0, 0},
};
static const LoomMode1Layer two_layers[2] = {
    {0u, 0u, 1u, 1u, 1u, 1u, 0, 0},
    {1u, 0u, 5u, 7u, 0u, 1u, 0, 0},
};
static const LoomMode1Layer drifting[2] = {
    {0u, 0u, 1u, 1u, 1u, 1u, 0, 0},
    {2u, 0u, 1u, 2u, 1u, 2u, 64, 0},
};

static LoomMode1ScrollCache cache;
static LoomDisplayState displays[CALLS];
static loom_u16 results[CALLS];

/* The asm variant calls no helper; this keeps tcc__mul, tcc__udiv and
 * tcc__div linked in every variant so helper_labels resolve. */
static short helper_sink;
static void link_helpers(unsigned short a, unsigned short b, short c)
{
    helper_sink = (short)(a * b + a / b + c / (short)b);
}

void bench_setup(void)
{
    unsigned short i, j;

    link_helpers(7u, 3u, -9);
    unsigned char *bytes;

    /* A fresh commit's display: every scroll zero; the rest a pattern the
     * block must leave alone. */
    for (i = 0; i < CALLS; i++) {
        bytes = (unsigned char *)&displays[i];
        for (j = 0; j < sizeof(LoomDisplayState); j++)
            bytes[j] = (unsigned char)(j < 16u ? 0u : 0xa0u + j + i);
    }
    bytes = (unsigned char *)&cache;
    for (j = 0; j < sizeof(LoomMode1ScrollCache); j++)
        bytes[j] = 0u;
}

void bench_run(void)
{
    results[0] = loom_pvs_mode1_scroll(&displays[0], &cache, parallax, 3u, 120, 64);
    results[1] = loom_pvs_mode1_scroll(&displays[1], &cache, parallax, 3u, 120, 64);
    results[2] = loom_pvs_mode1_scroll(&displays[2], &cache, parallax, 3u, -37, -5);
    results[3] = loom_pvs_mode1_scroll(&displays[3], &cache, two_layers, 2u, 1789, 413);
    results[4] = loom_pvs_mode1_scroll(&displays[4], &cache, two_layers, 2u, 1789, 413);
    results[5] = loom_pvs_mode1_scroll(&displays[5], &cache, parallax, 0u, 300, 20);
    results[6] = loom_pvs_mode1_scroll(&displays[6], &cache, drifting, 2u, 400, 30);
    results[7] = loom_pvs_mode1_scroll(&displays[7], &cache, parallax, 3u, 400, 30);
}

static unsigned short fold(const unsigned char *bytes, unsigned short count)
{
    unsigned short h = 0x1234u, i;
    for (i = 0; i < count; i++)
        h = (unsigned short)(((unsigned short)(h << 5) | (unsigned short)(h >> 11)) ^ bytes[i]);
    return h;
}

void bench_check(void)
{
    unsigned short i;
    for (i = 0; i < CALLS; i++)
        BENCH_OUT(results[i]);
    for (i = 0; i < CALLS; i++)
        BENCH_OUT(fold((const unsigned char *)&displays[i], 31u));
    BENCH_OUT(fold((const unsigned char *)&cache, 23u));
    for (i = 0; i < 4; i++)
        BENCH_OUT(displays[7].bg_scroll_x[i]);
    BENCH_OUT(displays[7].bg_scroll_y[2]);
    BENCH_OUT(displays[2].bg_scroll_x[2]);
    BENCH_OUT(displays[2].bg_scroll_y[1]);
    BENCH_OUT(displays[3].bg_scroll_x[1]);
}
