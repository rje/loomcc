#include "bench.h"

unsigned short sum_u16(const unsigned short *a, unsigned short n);
unsigned short sum_u8(const unsigned char *a, unsigned short n);
void fill_u8(unsigned char *d, unsigned char v, unsigned short n);
void fill_u16(unsigned short *d, unsigned short v, unsigned short n);
void copy_u8(unsigned char *d, const unsigned char *s, unsigned short n);
unsigned short nested(unsigned short rows, unsigned short cols);

static unsigned short words[64];
static unsigned char bytes[96];
static unsigned char dst8[80];
static unsigned short dst16[40];
static unsigned short r_sum16, r_sum8, r_nested;

void bench_setup(void)
{
    unsigned short i;
    for (i = 0; i < 64; i++)
        words[i] = (unsigned short)(i * 1031u + 7u);
    for (i = 0; i < 96; i++)
        bytes[i] = (unsigned char)(i * 37u + 3u);
}

void bench_run(void)
{
    r_sum16 = sum_u16(words, 32);
    r_sum8 = sum_u8(bytes, 40);
    fill_u8(dst8, 0x5a, 40);
    fill_u16(dst16, 0x1234, 24);
    copy_u8(dst8 + 8, bytes + 20, 24);
    r_nested = nested(8, 9);
}

void bench_check(void)
{
    unsigned short i, h8 = 0, h16 = 0;
    for (i = 0; i < 80; i++)
        h8 = (unsigned short)((h8 << 1 | h8 >> 15) ^ dst8[i]);
    for (i = 0; i < 40; i++)
        h16 = (unsigned short)((h16 << 3 | h16 >> 13) + dst16[i]);
    BENCH_OUT(r_sum16);
    BENCH_OUT(r_sum8);
    BENCH_OUT(r_nested);
    BENCH_OUT(h8);
    BENCH_OUT(h16);
    BENCH_OUT(dst8[7]);
    BENCH_OUT(dst8[8]);
    BENCH_OUT(dst8[31]);
    BENCH_OUT(dst8[32]);
}
