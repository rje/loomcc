#include "bench.h"

typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;

u16 chain(u16 a, u16 b);
u16 many_args(u8 layer, u16 x, u16 y, u8 tile, u8 palette, const u8 *attr, u16 w, u16 h, u8 flags);
void split(u16 v, u16 *hi, u16 *lo);
void minmax(const s16 *v, u16 n, s16 *lo, s16 *hi);
u16 run_hooks(const u8 *program, u16 n, u16 value);

static const u8 attr[4] = {9, 8, 7, 6};
static const u8 program[12] = {0, 1, 3, 2, 1, 1, 0, 3, 2, 0, 1, 3};
static s16 values[12];
static u16 out[16];

void bench_setup(void)
{
    u16 i;
    for (i = 0; i < 12; i++)
        values[i] = (s16)(i * 2749u - 15000u);
}

void bench_run(void)
{
    s16 lo, hi;
    out[0] = chain(100, 7);
    out[1] = chain(0xfff0, 0x1234);
    out[2] = many_args(1, 40, 17, 0x2c, 3, attr, 16, 8, 2);
    out[3] = many_args(3, 300, 200, 0xff, 7, attr, 1, 2, 5);
    split(0xbeef, &out[4], &out[5]);
    minmax(values, 12, &lo, &hi);
    out[6] = (u16)lo;
    out[7] = (u16)hi;
    out[8] = run_hooks(program, 12, 0x0102);
}

void bench_check(void)
{
    u16 i;
    for (i = 0; i < 9; i++)
        BENCH_OUT(out[i]);
}
