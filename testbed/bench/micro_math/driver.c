#include "bench.h"

typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;
typedef signed char s8;

s16 step_axis(s16 pos, u8 *sub, s16 vel, s16 lo, s16 hi);
u16 compare_mix(u16 a, u16 b);
u16 lfsr(u16 state, u16 steps);
u16 popcount(u16 v);
s16 abs_diff_sum(const s16 *a, const s16 *b, u16 n);
u8 sat_add_u8(u8 a, u8 b);
s16 sum_s8(const s8 *v, u16 n);

static s16 va[16], vb[16];
static s8 bytes[24];
static u8 sub;
static u16 out[24];

void bench_setup(void)
{
    u16 i;
    for (i = 0; i < 16; i++) {
        va[i] = (s16)(i * 997u - 5000u);
        vb[i] = (s16)(i * 311u + 20u);
    }
    for (i = 0; i < 24; i++)
        bytes[i] = (s8)(i * 29u - 100u);
    sub = 0x80;
}

void bench_run(void)
{
    s16 p = 100;
    p = step_axis(p, &sub, 0x0180, 0, 300);
    p = step_axis(p, &sub, -0x0240, 0, 300);
    p = step_axis(p, &sub, 0x7f40, 0, 300);
    out[0] = (u16)p;
    out[1] = (u16)step_axis(5, &sub, -0x0600, 0, 300);
    out[2] = compare_mix(5, 7);
    out[3] = compare_mix(0xfff0, 7);
    out[4] = compare_mix(7, 0xfff0);
    out[5] = compare_mix(0x8000, 0x7fff);
    out[6] = compare_mix(1234, 1234);
    out[7] = compare_mix(0xff00, 0);
    out[8] = lfsr(0xace1, 24);
    out[9] = popcount(0xbeef);
    out[10] = popcount(0x0001);
    out[11] = (u16)abs_diff_sum(va, vb, 16);
    out[12] = sat_add_u8(200, 100);
    out[13] = sat_add_u8(20, 100);
    out[14] = (u16)sum_s8(bytes, 24);
    out[15] = sub;
}

void bench_check(void)
{
    u16 i;
    for (i = 0; i < 16; i++)
        BENCH_OUT(out[i]);
}
