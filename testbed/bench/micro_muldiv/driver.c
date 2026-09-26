#include "bench.h"

typedef unsigned short u16;
typedef signed short s16;

u16 mul_const(u16 y, u16 x);
u16 mul_var_sum(const u16 *a, const u16 *b, u16 n);
s16 mul_signed(s16 a, s16 b);
u16 div_const(u16 v);
s16 div_signed_const(s16 v);
u16 div_var(u16 a, u16 b);
s16 div_var_signed(s16 a, s16 b);

static u16 va[8], vb[8];
static u16 out[24];

void bench_setup(void)
{
    u16 i;
    for (i = 0; i < 8; i++) {
        va[i] = (u16)(i * 409u + 17u);
        vb[i] = (u16)(i * 3u + 250u);
    }
}

void bench_run(void)
{
    out[0] = mul_const(7, 13);
    out[1] = mul_const(200, 1000);
    out[2] = mul_var_sum(va, vb, 8);
    out[3] = (u16)mul_signed(-123, 45);
    out[4] = (u16)mul_signed(-300, -300);
    out[5] = div_const(54321);
    out[6] = div_const(99);
    out[7] = (u16)div_signed_const(-1001);
    out[8] = (u16)div_signed_const(777);
    out[9] = div_var(60000, 7);
    out[10] = div_var(1234, 1234);
    out[11] = div_var(5, 300);
    out[12] = (u16)div_var_signed(-7001, 13);
    out[13] = (u16)div_var_signed(7001, -13);
    out[14] = (u16)div_var_signed(-32000, -9);
}

void bench_check(void)
{
    u16 i;
    for (i = 0; i < 15; i++)
        BENCH_OUT(out[i]);
}
