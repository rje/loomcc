/* Multiply and divide: by constants (powers of two and not), by variables,
 * signed and unsigned, and the modulo that goes with them. */

typedef unsigned short u16;
typedef signed short s16;

/* Row offsets: y * 20, y * 32, y * 24 + x: constant multipliers. */
u16 mul_const(u16 y, u16 x)
{
    return (u16)((u16)(y * 20u) + (u16)(y * 32u) + (u16)(y * 24u + x) + (u16)(x * 3u) + (u16)(x * 10u));
}

u16 mul_var_sum(const u16 *a, const u16 *b, u16 n)
{
    u16 s = 0, i;
    for (i = 0; i < n; i++)
        s = (u16)(s + (u16)(a[i] * b[i]));
    return s;
}

s16 mul_signed(s16 a, s16 b)
{
    return (s16)(a * b);
}

/* Division by constants: tile (/16), /10 digits, %10, /3, signed /4. */
u16 div_const(u16 v)
{
    u16 digits = 0;
    u16 t = v;
    while (t != 0) {
        digits = (u16)(digits + t % 10u);
        t = (u16)(t / 10u);
    }
    return (u16)((u16)(v / 16u) + (u16)(v / 3u) + (u16)(v % 7u) + digits);
}

s16 div_signed_const(s16 v)
{
    return (s16)(v / 4 + v / 3 + v % 5);
}

/* Division by variables: unsigned and signed, quotient and remainder. */
u16 div_var(u16 a, u16 b)
{
    return (u16)((u16)(a / b) * 7u + (u16)(a % b));
}

s16 div_var_signed(s16 a, s16 b)
{
    return (s16)((s16)(a / b) * 7 + (s16)(a % b));
}
