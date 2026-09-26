/* witness_hash: the debug witness's board hash (Stack's 10x20 board) over
 * the bottom seven rows of a board early in a game (a ragged five-row
 * stack) and the bottom two rows of a late one (kinds 1..7, with holes).
 * Two calls over 90 cells: through 816-tcc a cell costs about 2,500 master
 * clocks (a tcc__mul each), so a whole 200-cell board would not fit the
 * one-frame budget. */
#include "bench.h"

typedef unsigned char loom_u8;
typedef unsigned short loom_u16;

loom_u16 loom_pvs_board_hash(const loom_u8 *cells, loom_u16 count);

#define W 10
#define H 20

static loom_u8 early[W * H];
static loom_u8 late[W * H];
static loom_u16 results[2];

void bench_setup(void)
{
    static const char *const rows[5] = {
        "JJ........",
        "LLLS...T..",
        "IIIIZ.TTTO",
        "OO.ZZSSLLO",
        "TTTZ.SSLL.",
    };
    unsigned short x, y, i;
    for (i = 0; i < W * H; i++) {
        early[i] = 0;
        late[i] = 0;
    }
    for (y = 0; y < 5; y++)
        for (x = 0; x < W; x++) {
            char c = rows[y][x];
            early[(15 + y) * W + x] =
                c == '.' ? 0 : c == 'I' ? 1 : c == 'O' ? 2 : c == 'T' ? 3 : c == 'S' ? 4
                : c == 'Z' ? 5 : c == 'L' ? 6 : 7;
        }
    for (y = 6; y < H; y++)
        for (x = 0; x < W; x++)
            late[y * W + x] = (loom_u8)(((x * 3u + y * 5u) % 9u == 4u) ? 0 : 1 + (x + y * 2u) % 7u);
}

void bench_run(void)
{
    results[0] = loom_pvs_board_hash(early + W * 13, W * 7);
    results[1] = loom_pvs_board_hash(late + W * 18, W * 2);
}

void bench_check(void)
{
    BENCH_OUT(results[0]);
    BENCH_OUT(results[1]);
}
