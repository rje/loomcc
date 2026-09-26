/* board_fill: loom_board_clear's wipe on two small boards whose every cell
 * is filled, so every cell changes: a 4x2 next-piece box (cell_size 1, a
 * 4x2-tile surface) filled solid by a game-over wall, and a 2x1 board of
 * 2x2-tile cells (a 4x2-tile surface) of mixed kinds. Both keep a valid
 * hash that the wipe marks stale, and one of the box's rows is already
 * dirty. Two calls over 10 cells: through 816-tcc a cell of the wipe costs
 * about 16,000 master clocks, so Stack's 10x20 well would take several
 * frames. bench_check folds the cells, shadow words, dirty spans and
 * pending counts. (The C redraws every row whole and the assembly only
 * changed cells, so a board with empty cells would leave different spans:
 * see unit.c.) */
#include "bench.h"
#include "loom_board.h"

void loom_pvs_board_fill(const LoomBoardPaint *paint, loom_u16 kind);

#define W1 4
#define H1 2
#define W2 2
#define H2 1

static loom_u8 cells1[W1 * H1];
static loom_u16 kind_words1[8];
static loom_u16 shadow1[W1 * H1];
static loom_u8 first1[32];
static loom_u8 last1[32];
static loom_u16 pending1;
static LoomBoardPaint paint1;

static loom_u8 cells2[W2 * H2];
static loom_u16 kind_words2[8 * 4];
static loom_u16 shadow2[W2 * 2 * H2 * 2];
static loom_u8 first2[32];
static loom_u8 last2[32];
static loom_u16 pending2;
static LoomBoardPaint paint2;

void bench_setup(void)
{
    unsigned short i, x, y, s;
    for (i = 0; i < W1 * H1; i++)
        cells1[i] = 7;
    for (i = 0; i < 8; i++)
        kind_words1[i] = (loom_u16)(0x2400u + i * 2u);
    for (i = 0; i < W1 * H1; i++)
        shadow1[i] = kind_words1[7];
    for (i = 0; i < W2 * H2; i++)
        cells2[i] = (loom_u8)(1u + (i * 3u) % 7u);
    for (i = 0; i < 8 * 4; i++)
        kind_words2[i] = (loom_u16)(0x2800u + i);
    for (y = 0; y < H2; y++)
        for (s = 0; s < 2; s++)
            for (x = 0; x < W2 * 2; x++)
                shadow2[(y * 2 + s) * W2 * 2 + x] =
                    kind_words2[cells2[y * W2 + x / 2] * 4u + s * 2u + (x & 1u)];
    for (i = 0; i < 32; i++) {
        first1[i] = 0xff;
        last1[i] = 0;
        first2[i] = 0xff;
        last2[i] = 0;
    }
    first1[1] = 1; /* a row already dirty this tick */
    last1[1] = 2;
    pending1 = 2;
    pending2 = 0;

    paint1.cells = cells1;
    paint1.kind_words = kind_words1;
    paint1.shadow = shadow1;
    paint1.first = first1;
    paint1.last = last1;
    paint1.pending = &pending1;
    paint1.width = W1;
    paint1.height = H1;
    paint1.cell_size = 1;
    paint1.stride = W1;
    paint1.hash = 0x1234;
    paint1.hash_valid = 1;

    paint2.cells = cells2;
    paint2.kind_words = kind_words2;
    paint2.shadow = shadow2;
    paint2.first = first2;
    paint2.last = last2;
    paint2.pending = &pending2;
    paint2.width = W2;
    paint2.height = H2;
    paint2.cell_size = 2;
    paint2.stride = W2 * 2;
    paint2.hash = 0x4321;
    paint2.hash_valid = 1;
}

void bench_run(void)
{
    loom_pvs_board_fill(&paint1, 0);
    loom_pvs_board_fill(&paint2, 0);
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
    BENCH_OUT(fold8(cells1, W1 * H1));
    BENCH_OUT(fold16(shadow1, W1 * H1));
    BENCH_OUT(fold8(first1, 32));
    BENCH_OUT(fold8(last1, 32));
    BENCH_OUT(pending1);
    BENCH_OUT(paint1.hash);
    BENCH_OUT(paint1.hash_valid);
    BENCH_OUT(fold8(cells2, W2 * H2));
    BENCH_OUT(fold16(shadow2, W2 * 2 * H2 * 2));
    BENCH_OUT(fold8(first2, 32));
    BENCH_OUT(fold8(last2, 32));
    BENCH_OUT(pending2);
    BENCH_OUT(paint2.hash);
    BENCH_OUT(paint2.hash_valid);
    BENCH_OUT(shadow1[5]);
    BENCH_OUT(shadow2[7]);
    BENCH_OUT(first1[1]);
    BENCH_OUT(last1[1]);
    BENCH_OUT(first2[1]);
    BENCH_OUT(last2[1]);
}
