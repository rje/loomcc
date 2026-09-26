/* scene_trigger_scan: a 512x448 room with ten triggers (doors on the edges,
 * a sign, a chest, pressure plates, a wide zone) tested against eight
 * player boxes the way loom_scene_check_triggers does once a tick: most
 * boxes hit nothing, some sit on one trigger, some on overlapping ones, some
 * touch an edge exactly (the open-interval rule), one is partly off the map,
 * and one call has no triggers at all. */
#include "bench.h"
#include "loom_scene.h"

#define TRIGGERS 10
#define PROBES 8

static LoomSceneTrigger triggers[TRIGGERS];
static LoomSceneBox boxes[PROBES];
static loom_u16 results[PROBES + 1];

static void trigger_at(loom_u8 i, loom_s16 x, loom_s16 y, loom_u16 w, loom_u16 h)
{
    triggers[i].x = x;
    triggers[i].y = y;
    triggers[i].width = w;
    triggers[i].height = h;
    triggers[i].first_hook = (loom_u16)(i * 3u);
    triggers[i].target_scene = 0xffffu;
    triggers[i].hook_count = 1u;
    triggers[i].flags = (loom_u8)(i & 1u);
}

static void box_at(loom_u8 i, loom_s16 x, loom_s16 y)
{
    /* The player's collider: 12 wide, 14 tall. */
    boxes[i].left = x;
    boxes[i].top = y;
    boxes[i].right = (loom_s16)(x + 12);
    boxes[i].bottom = (loom_s16)(y + 14);
}

void bench_setup(void)
{
    trigger_at(0, 240, 0, 32, 8);     /* north door */
    trigger_at(1, 0, 200, 8, 48);     /* west door */
    trigger_at(2, 504, 200, 8, 48);   /* east door */
    trigger_at(3, 240, 440, 32, 8);   /* south door */
    trigger_at(4, 96, 96, 16, 16);    /* sign */
    trigger_at(5, 384, 128, 16, 16);  /* chest */
    trigger_at(6, 160, 300, 24, 24);  /* plate */
    trigger_at(7, 176, 310, 24, 24);  /* plate overlapping the first */
    trigger_at(8, 64, 256, 320, 64);  /* wide zone */
    trigger_at(9, -16, -16, 32, 32);  /* corner, partly off the map */

    box_at(0, 300, 180);   /* open floor */
    box_at(1, 250, 2);     /* north door */
    box_at(2, 100, 90);    /* sign */
    box_at(3, 170, 305);   /* both plates and the zone */
    box_at(4, 84, 96);     /* right edge touches the sign's left: open */
    box_at(5, 390, 120);   /* chest */
    box_at(6, -6, -4);     /* corner trigger */
    box_at(7, 60, 250);    /* zone corner */
}

#define P(i) results[i] = loom_pvs_scene_intersect_mask(&boxes[i], triggers, TRIGGERS)

void bench_run(void)
{
    P(0);
    P(1);
    P(2);
    P(3);
    P(4);
    P(5);
    P(6);
    P(7);
    results[PROBES] = loom_pvs_scene_intersect_mask(&boxes[3], triggers, 0);
}

void bench_check(void)
{
    loom_u8 i;

    for (i = 0; i <= PROBES; i++)
        BENCH_OUT(results[i]);
}
