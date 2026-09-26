#include "bench.h"

typedef unsigned short u16;
typedef unsigned char u8;

u16 dense(u16 op, u16 a, u16 b);
u16 sparse(u16 key);
u16 tokens(const u8 *s, u16 n);

static const u8 text[] = "go 12 north, then 3east and 40 west..";
static u16 out[3];
static u16 sp[16];

void bench_setup(void) {}

#define D(op) acc = (u16)(acc + dense(op, acc, (u16)((op) * 1111u)))
#define S(i, key) sp[i] = sparse(key)

void bench_run(void)
{
    u16 acc = 0x1234;
    D(0); D(1); D(2); D(3); D(4); D(5); D(6); D(7); D(8); D(9); D(10); D(11); D(12);
    out[0] = acc;
    S(0, 3); S(1, 4); S(2, 40); S(3, 41); S(4, 42); S(5, 500); S(6, 0x1000); S(7, 0x0fff);
    S(8, 0x7fff); S(9, 0x8000); S(10, 0x8001); S(11, 0xfffe); S(12, 0xffff); S(13, 0); S(14, 1); S(15, 499);
    out[2] = tokens(text, sizeof(text) - 1);
}

void bench_check(void)
{
    u16 i, s = 0;
    for (i = 0; i < 16; i++)
        s = (u16)(s * 3u + sp[i]);
    out[1] = s;
    BENCH_OUT(out[0]);
    BENCH_OUT(out[1]);
    BENCH_OUT(out[2]);
}
