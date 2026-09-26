/* movement_box_blocked: a 20x14-cell room (320x224 pixels, 16-pixel tiles)
 * with a border wall and a few blocks, probed with 24 boxes (constant arguments, so bench_run is only the calls) the way a tick
 * probes the player and the actors: clear boxes (the full scan), boxes that
 * touch a wall, boxes off the map, boxes straddling tiles. */
#include "bench.h"

typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

loom_u8 loom_pvs_movement_box_blocked(const loom_u8 *cells,
                                      loom_u16 collision_width,
                                      loom_u16 pixel_width,
                                      loom_u16 pixel_height,
                                      loom_s16 left, loom_s16 top,
                                      loom_s16 right, loom_s16 bottom);

#define W 20
#define H 14
#define QUERIES 24

static loom_u8 cells[W * H];
static loom_u8 results[QUERIES];

void bench_setup(void)
{
    unsigned short x, y;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            cells[y * W + x] = (x == 0 || y == 0 || x == W - 1 || y == H - 1) ? 1 : 0;
    cells[5 * W + 5] = 1;  /* a block at (80..95, 80..95) */
    cells[7 * W + 12] = 1; /* a block at (192..207, 112..127) */
    cells[3 * W + 11] = 2;
    cells[10 * W + 15] = 1;
}

/* left, top, width, height */
#define Q(i, l, t, w, h) \
    results[i] = loom_pvs_movement_box_blocked(cells, W, W * 16, H * 16, (l), (t), \
                                               (loom_s16)((l) + (w) - 1), (loom_s16)((t) + (h) - 1))

void bench_run(void)
{
    Q(0, 40, 40, 16, 24);
    Q(1, 100, 60, 12, 30);
    Q(2, 150, 120, 16, 16);
    Q(3, 17, 17, 14, 14);
    Q(4, 8, 40, 16, 16);
    Q(5, 40, 8, 16, 16);
    Q(6, 300, 100, 16, 16);
    Q(7, 100, 200, 16, 16);
    Q(8, -4, 50, 16, 16);
    Q(9, 50, -1, 16, 16);
    Q(10, 310, 50, 16, 16);
    Q(11, 50, 220, 16, 16);
    Q(12, 64, 64, 32, 32);
    Q(13, 95, 95, 2, 2);
    Q(14, 160, 48, 40, 60);
    Q(15, 200, 150, 30, 40);
    Q(16, 32, 176, 250, 15);
    Q(17, 33, 33, 200, 100);
    Q(18, 240, 32, 47, 47);
    Q(19, 128, 128, 1, 1);
    Q(20, 16, 16, 288, 192);
    Q(21, 250, 170, 40, 20);
    Q(22, 111, 33, 16, 16);
    Q(23, 176, 96, 16, 16);
}

void bench_check(void)
{
    unsigned short i, word = 0;
    for (i = 0; i < QUERIES; i++) {
        word |= (unsigned short)((results[i] & 1u) << (i & 15u));
        if ((i & 15u) == 15u || i == QUERIES - 1) {
            BENCH_OUT(word);
            word = 0;
        }
    }
    for (i = 0; i < QUERIES; i++)
        BENCH_OUT(results[i]);
}
