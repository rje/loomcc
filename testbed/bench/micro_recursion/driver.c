#include "bench.h"

typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;

#define GW 4
#define GH 3
extern u8 grid[GH][GW];

typedef struct {
    s16 value;
    u8 left, right;
} Node;

u16 flood(u8 x, u8 y, u8 to);
s16 eval(const u8 *s);
s16 tree_sum(const Node *nodes, u8 i, u16 depth, u16 *max_depth);

static const u8 walls[GH][GW] = {
    {0, 0, 1, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
};
static const u8 e1[] = "(1+2)*(3-4*-5)+6";
static const u8 e2[] = "2*(3-(4-1))-(7*-2)";
static Node nodes[7];
static u16 out[8];

void bench_setup(void)
{
    u8 i;
    for (i = 0; i < 7; i++) {
        nodes[i].value = (s16)(i * 37 - 200);
        nodes[i].left = (u8)(2 * i + 1 < 7 ? 2 * i + 1 : 0xff);
        nodes[i].right = (u8)(2 * i + 2 < 7 ? 2 * i + 2 : 0xff);
    }
}

void bench_run(void)
{
    u8 x, y;
    u16 depth = 0;
    for (y = 0; y < GH; y++)
        for (x = 0; x < GW; x++)
            grid[y][x] = walls[y][x];
    out[0] = flood(0, 0, 2);
    out[1] = flood(3, 2, 3);
    out[2] = flood(0, 0, 2);
    out[3] = grid[2][0] | (u16)(grid[2][3] << 8);
    out[4] = (u16)eval(e1);
    out[5] = (u16)eval(e2);
    out[6] = (u16)tree_sum(nodes, 0, 0, &depth);
    out[7] = depth;
}

void bench_check(void)
{
    u16 i;
    for (i = 0; i < 8; i++)
        BENCH_OUT(out[i]);
}
