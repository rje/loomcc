/* board_fit: a 10x20 falling-piece board (Stack's) with a ragged stack in
 * its bottom five rows, probed with 7 tetromino fit tests the way a tick's
 * fall step, slide and turn (and their wall kicks) ask: pieces spawning
 * partly above the top, open air, resting on the stack, into it, through
 * both walls and the floor (7 calls: a fit test
 * costs 816-tcc up to 40,000 master clocks). Constant arguments, so bench_run is only the
 * calls. */
#include "bench.h"
#include "loom_board.h"

loom_u16 loom_pvs_board_fits(const LoomBoardPaint *paint, loom_u16 shape, loom_s16 x,
                             loom_s16 y);

#define W 10
#define H 20
#define QUERIES 7

/* Shapes: bit row * 4 + column, row 0 the top. */
#define I_FLAT 0x00f0u
#define I_TALL 0x2222u
#define O_SQ 0x0066u
#define T_UP 0x0072u
#define S_FLAT 0x0036u
#define Z_TALL 0x0132u
#define L_UP 0x0074u
#define J_TALL 0x0226u

static loom_u8 board_cells[W * H];
static loom_u16 kind_words[8];
static loom_u16 shadow[W * H];
static loom_u8 first[32];
static loom_u8 last[32];
static loom_u16 pending;
static LoomBoardPaint paint;
static loom_u16 results[QUERIES];

void bench_setup(void)
{
    static const char *const rows[5] = {
        "JJ........",
        "LLLS...T..",
        "IIIIZ.TTTO",
        "OO.ZZSSLLO",
        "TTTZ.SSLL.",
    };
    unsigned short x, y;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            board_cells[y * W + x] = 0;
    for (y = 0; y < 5; y++)
        for (x = 0; x < W; x++) {
            char c = rows[y][x];
            board_cells[(15 + y) * W + x] =
                c == '.' ? 0 : c == 'I' ? 1 : c == 'O' ? 2 : c == 'T' ? 3 : c == 'S' ? 4
                : c == 'Z' ? 5 : c == 'L' ? 6 : 7;
        }
    for (y = 0; y < 32; y++) {
        first[y] = 0xff;
        last[y] = 0;
    }
    paint.cells = board_cells;
    paint.kind_words = kind_words;
    paint.shadow = shadow;
    paint.first = first;
    paint.last = last;
    paint.pending = &pending;
    paint.width = W;
    paint.height = H;
    paint.cell_size = 1;
    paint.stride = W;
    paint.hash = 0;
    paint.hash_valid = 0;
}

#define Q(i, shape, x, y) results[i] = loom_pvs_board_fits(&paint, (shape), (x), (y))

void bench_run(void)
{
    Q(0, T_UP, 3, -1);    /* spawn, partly above the top */
    Q(1, L_UP, 4, 13);    /* a fall step onto the stack */
    Q(2, O_SQ, 4, 16);    /* into it */
    Q(3, I_TALL, -1, 10); /* its empty left column over the wall */
    Q(4, I_FLAT, 7, 3);   /* the right wall */
    Q(5, O_SQ, 7, 18);    /* through the floor */
    Q(6, J_TALL, 7, -3);  /* wholly above the board */
}

void bench_check(void)
{
    unsigned short i, word = 0;
    for (i = 0; i < QUERIES; i++)
        word |= (unsigned short)((results[i] & 1u) << (i & 15u));
    BENCH_OUT(word);
    for (i = 0; i < QUERIES; i++)
        BENCH_OUT(results[i]);
}
