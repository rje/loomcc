/* board_drop: a hard drop on Stack's 10x20 board (cell_size 1, drawn
 * through a 10x20-tile surface) in a release build (no kept hash): a
 * two-cell piece (a column game's pair) lifted from (5, 15), falling to the
 * lowest row it fits straight below (one row: two fit tests) and stamped
 * there. One call: through 816-tcc a written cell costs about 50,000 master
 * clocks and a fit test up to 35,000, so a longer fall, a tetromino or the
 * kept hash would not fit the one-frame budget. bench_check folds the
 * cells, shadow words, dirty spans, pending count, hash and rows fallen. */
#include "bench.h"
#include "loom_board.h"

loom_u16 loom_pvs_board_drop(const LoomBoardPaint *paint, loom_u16 shape, loom_s16 x,
                             loom_s16 y, loom_u16 kind);

#define W 10
#define H 20

#define PAIR_TALL 0x0011u /* two cells, one above the other */

static loom_u8 cells[W * H];
static loom_u16 kind_words[8];
static loom_u16 shadow[W * H];
static loom_u8 first[32];
static loom_u8 last[32];
static loom_u16 pending;
static LoomBoardPaint paint;


static loom_u16 weighted(const loom_u8 *c, loom_u16 n)
{
    loom_u16 i, h = 0;
    for (i = 0; i < n; i++)
        h = (loom_u16)(h + (loom_u16)(c[i] * (loom_u16)(2u * i + 1u)));
    return h;
}

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
    for (i = 0; i < W * H; i++)
        cells[i] = 0;
    for (y = 0; y < 5; y++)
        for (x = 0; x < W; x++) {
            char c = rows[y][x];
            cells[(15 + y) * W + x] =
                c == '.' ? 0 : c == 'I' ? 1 : c == 'O' ? 2 : c == 'T' ? 3 : c == 'S' ? 4
                : c == 'Z' ? 5 : c == 'L' ? 6 : 7;
        }
    /* The falling pair at (5, 15) and (5, 16), kind 4. */
    cells[15 * W + 5] = 4;
    cells[16 * W + 5] = 4;
    for (i = 0; i < 8; i++)
        kind_words[i] = (loom_u16)(0x2400u + i * 2u);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            shadow[y * W + x] = kind_words[cells[y * W + x]];
    for (i = 0; i < 32; i++) {
        first[i] = 0xff;
        last[i] = 0;
    }
    first[16] = 5; /* the pair's last move is not flushed yet */
    last[16] = 5;
    pending = 1;
    paint.cells = cells;
    paint.kind_words = kind_words;
    paint.shadow = shadow;
    paint.first = first;
    paint.last = last;
    paint.pending = &pending;
    paint.width = W;
    paint.height = H;
    paint.cell_size = 1;
    paint.stride = W;
    paint.hash = weighted(cells, W * H);
    paint.hash_valid = 0; /* a release build: no witness asks for the hash */

}

static loom_u16 rows;

void bench_run(void)
{
    rows = loom_pvs_board_drop(&paint, PAIR_TALL, 5, 15, 4);
}

static loom_u16 fold8(const loom_u8 *p, loom_u16 n)
{
    loom_u16 i, h = 5381u;
    for (i = 0; i < n; i++)
        h = (loom_u16)((loom_u16)(h << 5) + h + p[i]);
    return h;
}

static loom_u16 fold16(const loom_u16 *p, loom_u16 n)
{
    loom_u16 i, h = 5381u;
    for (i = 0; i < n; i++)
        h = (loom_u16)((loom_u16)((loom_u16)(h << 5) + h) ^ p[i]);
    return h;
}

void bench_check(void)
{
    BENCH_OUT(fold8(cells, W * H));
    BENCH_OUT(fold16(shadow, W * H));
    BENCH_OUT(fold8(first, 32));
    BENCH_OUT(fold8(last, 32));
    BENCH_OUT(pending);
    BENCH_OUT(paint.hash);
    BENCH_OUT(paint.hash_valid);
    BENCH_OUT(weighted(cells, W * H));
    BENCH_OUT(rows);
    BENCH_OUT(first[15]);
    BENCH_OUT(last[15]);
    BENCH_OUT(first[16]);
    BENCH_OUT(first[17]);
}
