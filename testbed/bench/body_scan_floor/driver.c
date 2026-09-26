/* body_scan_floor: the floor scan under a 12-pixel-wide body's feet over a
 * 24x14-cell room, 14 calls with constant arguments (left, right, feet,
 * reach, sensor_x = left + 6): standing on the ground (grounded reach),
 * falling through open air, landing on and falling through one-way
 * ledges, slopes both ways under the sensor, a slope past the reach, a
 * solid cell under the box's far column, a fast fall across six rows,
 * and scans off the map's bottom and left. */
#include "bench.h"
#include "loom_body.h"
#include "grid.h"

#define QUERIES 14

static loom_s16 results[QUERIES];

void bench_setup(void)
{
    grid_build();
}

#define Q(i, l, f, r, s) results[i] = loom_pvs_body_scan_floor((l), (loom_s16)((l) + 11), (f), (r), (s))

void bench_run(void)
{
    Q(0, 40, 207, 223, 46);
    Q(1, 40, 100, 110, 46);
    Q(2, 100, 108, 118, 106);
    Q(3, 100, 115, 125, 106);
    Q(4, 194, 165, 181, 200);
    Q(5, 270, 150, 170, 276);
    Q(6, 204, 130, 150, 210);
    Q(7, 150, 170, 190, 156);
    Q(8, 20, 20, 120, 26);
    Q(9, 250, 40, 60, 256);
    Q(10, 40, 230, 240, 46);
    Q(11, -8, 100, 104, -2);
    Q(12, 210, 150, 166, 216);
    Q(13, 60, 190, 206, 66);
}

void bench_check(void)
{
    unsigned short i;
    for (i = 0; i < QUERIES; i++)
        BENCH_OUT((loom_u16)results[i]);
}
