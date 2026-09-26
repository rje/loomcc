#include "bench.h"

extern unsigned char map[12][16];
extern unsigned short cost[8][8];
extern unsigned short hist[16];
extern unsigned char line_u8[32];
extern unsigned short line_u16[32];
unsigned short map_weight(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1);
void cost_sweep(void);
void histogram(unsigned short n);
void reverse_u16(unsigned short n);

static unsigned short r_w1, r_w2;

void bench_setup(void)
{
    unsigned short x, y;
    for (y = 0; y < 12; y++)
        for (x = 0; x < 16; x++)
            map[y][x] = (unsigned char)((x * 5u + y * 3u) ^ (x & y));
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            cost[y][x] = (unsigned short)((x * 7u + y * 13u) % 11u + 1u);
    for (x = 0; x < 32; x++) {
        line_u8[x] = (unsigned char)(x * 53u + 11u);
        line_u16[x] = (unsigned short)(x * 1000u + 3u);
    }
}

void bench_run(void)
{
    r_w1 = map_weight(2, 1, 10, 6);
    r_w2 = map_weight(0, 0, 16, 1);
    cost_sweep();
    histogram(16);
    reverse_u16(21);
}

void bench_check(void)
{
    unsigned short i, h = 0;
    BENCH_OUT(r_w1);
    BENCH_OUT(r_w2);
    for (i = 0; i < 8; i++)
        BENCH_OUT(cost[i][5]);
    BENCH_OUT(cost[7][3]);
    for (i = 0; i < 16; i++)
        h = (unsigned short)(h * 3u + hist[i]);
    BENCH_OUT(h);
    for (i = 0; i < 32; i++)
        h = (unsigned short)((h << 1 | h >> 15) ^ line_u16[i]);
    BENCH_OUT(h);
    BENCH_OUT(line_u16[0]);
    BENCH_OUT(line_u16[20]);
}
