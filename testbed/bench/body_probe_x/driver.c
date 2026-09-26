/* body_probe_x: the wall probe along X over a 24x14-cell room, 12 calls
 * with constant arguments (edge, delta, top, wall_bottom) the way a body's
 * X move asks it: short steps into blocks and the border, open air, long
 * sweeps that step by tiles, one-way and slope cells (not walls), probes
 * off the map and rows above it, an empty row range. */
#include "bench.h"
#include "loom_body.h"
#include "grid.h"

#define QUERIES 12

static loom_s16 results[QUERIES];

void bench_setup(void)
{
    grid_build();
}

#define Q(i, e, d, t, b) results[i] = loom_pvs_body_probe_x((e), (d), (t), (b))

void bench_run(void)
{
    Q(0, 100, 3, 40, 63);
    Q(1, 79, 4, 40, 63);
    Q(2, 130, -4, 48, 60);
    Q(3, 360, 8, 100, 130);
    Q(4, 20, -6, 100, 130);
    Q(5, 230, 60, 112, 140);
    Q(6, 90, 20, 112, 127);
    Q(7, 200, 30, 150, 158);
    Q(8, -3, -4, 20, 40);
    Q(9, 100, 4, -20, 10);
    Q(10, 100, 4, 60, 50);
    Q(11, 193, -1, 80, 95);
}

void bench_check(void)
{
    unsigned short i;
    for (i = 0; i < QUERIES; i++)
        BENCH_OUT((loom_u16)results[i]);
}
