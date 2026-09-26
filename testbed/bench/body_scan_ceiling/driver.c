/* body_scan_ceiling: the ceiling scan over a body's head over a 24x14-cell
 * room, 11 calls with constant arguments (left, right, head, reach): a jump
 * into a block, open air, a one-way ledge and a slope (neither is a
 * ceiling), the border row, a block under the box's far column, a long rise
 * through open rows, a wide box, and scans off the map's right and left. */
#include "bench.h"
#include "loom_body.h"
#include "grid.h"

#define QUERIES 11

static loom_s16 results[QUERIES];

void bench_setup(void)
{
    grid_build();
}

#define Q(i, l, r, h, re) results[i] = loom_pvs_body_scan_ceiling((l), (r), (h), (re))

void bench_run(void)
{
    Q(0, 90, 101, 70, 60);
    Q(1, 40, 51, 100, 92);
    Q(2, 100, 111, 130, 105);
    Q(3, 20, 31, 20, 5);
    Q(4, 170, 181, 110, 90);
    Q(5, 280, 311, 144, 120);
    Q(6, 36, 47, 200, 100);
    Q(7, 196, 207, 176, 150);
    Q(8, 378, 389, 50, 40);
    Q(9, -6, 5, 50, 40);
    Q(10, 277, 288, 140, 118);
}

void bench_check(void)
{
    unsigned short i;
    for (i = 0; i < QUERIES; i++)
        BENCH_OUT((loom_u16)results[i]);
}
