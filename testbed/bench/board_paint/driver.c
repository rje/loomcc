/* board_paint: a falling-piece game's stamp on Stack's 10x20 board
 * (cell_size 1, drawn through a 10x20-tile surface), with the debug
 * witness's kept hash valid and one of the piece's rows already dirty from
 * its lift earlier in the tick: a T stamped over a cell already of its kind
 * (three cells written, one skipped), then an I spawning wholly above the
 * top (every cell clipped). Two calls: through 816-tcc a written cell costs
 * about 50,000 master clocks, so a whole fall step (lift and stamp) would
 * not fit the one-frame budget. bench_check folds the cells, shadow words,
 * dirty spans, pending count and hash. */
#include "bench.h"
#include "loom_board.h"

void loom_pvs_board_paint(const LoomBoardPaint *paint, loom_u16 shape, loom_s16 x,
                          loom_s16 y, loom_u16 kind);

#define W 10
#define H 20

#define I_FLAT 0x00f0u
#define T_UP 0x0072u

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
    /* A T cell at (5, 6) already: the stamp leaves it as it is. */
    cells[6 * W + 5] = 3;
    cells[13 * W + 1] = 6;
    cells[14 * W + 0] = 6;
    cells[14 * W + 1] = 6;
    cells[14 * W + 5] = 2;
    for (i = 0; i < 8; i++)
        kind_words[i] = (loom_u16)(0x2400u + i * 2u);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            shadow[y * W + x] = kind_words[cells[y * W + x]];
    for (i = 0; i < 32; i++) {
        first[i] = 0xff;
        last[i] = 0;
    }
    first[5] = 6; /* a row already dirty this tick */
    last[5] = 7;
    pending = 2;
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
    paint.hash_valid = 1;

}

void bench_run(void)
{
    loom_pvs_board_paint(&paint, T_UP, 3, 5, 3);     /* the T a row lower */
    loom_pvs_board_paint(&paint, I_FLAT, 3, -2, 1);  /* an I spawning above the top */
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
    BENCH_OUT(first[0]);
    BENCH_OUT(last[0]);
    BENCH_OUT(first[5]);
    BENCH_OUT(last[5]);
    BENCH_OUT(first[13]);
    BENCH_OUT(last[14]);
}
