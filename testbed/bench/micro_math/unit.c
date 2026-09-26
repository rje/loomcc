/* 16-bit arithmetic: add/sub/shift/logic mixes, signed and unsigned
 * compares, clamps, absolute values, 8-bit arithmetic. Every result is
 * narrowed explicitly so a 32-bit-int host computes the same words. */

typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;
typedef signed char s8;

/* A position update with a sub-pixel byte: the kind of code every tick runs. */
s16 step_axis(s16 pos, u8 *sub, s16 vel, s16 lo, s16 hi)
{
    u16 acc = (u16)(*sub + (u16)(vel & 0xff));
    *sub = (u8)acc;
    pos = (s16)(pos + (vel >> 8) + (s16)(acc >> 8));
    if (pos < lo)
        pos = lo;
    else if (pos > hi)
        pos = hi;
    return pos;
}

/* Signed and unsigned compares of the same bit patterns. */
u16 compare_mix(u16 a, u16 b)
{
    u16 r = 0;
    if (a < b) r |= 1u;
    if ((s16)a < (s16)b) r |= 2u;
    if (a >= b) r |= 4u;
    if ((s16)a >= (s16)b) r |= 8u;
    if (a == b) r |= 16u;
    if ((s16)a > 0) r |= 32u;
    if ((s16)a <= -100) r |= 64u;
    if (a > 1000u) r |= 128u;
    if (a != 0 && b == 0) r |= 256u;
    return r;
}

/* Shifts and logic: an LFSR, a bit reverse, a popcount. */
u16 lfsr(u16 state, u16 steps)
{
    while (steps--) {
        u16 bit = (u16)(((state >> 0) ^ (state >> 2) ^ (state >> 3) ^ (state >> 5)) & 1u);
        state = (u16)((state >> 1) | (bit << 15));
    }
    return state;
}

u16 popcount(u16 v)
{
    u16 n = 0;
    while (v) {
        v &= (u16)(v - 1u);
        n++;
    }
    return n;
}

s16 abs_diff_sum(const s16 *a, const s16 *b, u16 n)
{
    s16 total = 0;
    u16 i;
    for (i = 0; i < n; i++) {
        s16 d = (s16)(a[i] - b[i]);
        total = (s16)(total + (d < 0 ? -d : d));
    }
    return total;
}

/* 8-bit arithmetic: a saturating add and a signed-char accumulate. */
u8 sat_add_u8(u8 a, u8 b)
{
    u16 s = (u16)(a + b);
    return s > 255u ? 255u : (u8)s;
}

s16 sum_s8(const s8 *v, u16 n)
{
    s16 s = 0;
    while (n--)
        s = (s16)(s + *v++);
    return s;
}
