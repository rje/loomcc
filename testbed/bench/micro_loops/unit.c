/* Loops: sum an array (u16 and u8), a memset-like fill, a copy, nested loops. */

unsigned short sum_u16(const unsigned short *a, unsigned short n)
{
    unsigned short s = 0;
    unsigned short i;
    for (i = 0; i < n; i++)
        s += a[i];
    return s;
}

unsigned short sum_u8(const unsigned char *a, unsigned short n)
{
    unsigned short s = 0;
    while (n != 0) {
        s += *a++;
        n--;
    }
    return s;
}

void fill_u8(unsigned char *d, unsigned char v, unsigned short n)
{
    unsigned short i;
    for (i = 0; i < n; i++)
        d[i] = v;
}

void fill_u16(unsigned short *d, unsigned short v, unsigned short n)
{
    do {
        *d++ = v;
    } while (--n != 0);
}

void copy_u8(unsigned char *d, const unsigned char *s, unsigned short n)
{
    unsigned short i;
    for (i = 0; i < n; i++)
        d[i] = s[i];
}

/* A triangle walk: the inner bound depends on the outer index. */
unsigned short nested(unsigned short rows, unsigned short cols)
{
    unsigned short r, c, acc = 0;
    for (r = 0; r < rows; r++)
        for (c = r; c < cols; c++)
            acc = (unsigned short)(acc + (r ^ c) + 1u);
    return acc;
}
