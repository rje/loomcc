/* Calls: a chain of small functions, a call with many arguments (8-bit,
 * 16-bit and pointer slots), out-parameters through pointers, and calls
 * through a table of function pointers. */

typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;

static u16 leaf_add(u16 a, u16 b) { return (u16)(a + b); }
static u16 leaf_twice(u16 a) { return leaf_add(a, a); }
static u16 mid(u16 a, u16 b) { return (u16)(leaf_twice(a) ^ leaf_add(b, 3)); }
u16 chain(u16 a, u16 b) { return mid(mid(a, b), mid(b, a)); }

/* Nine arguments of mixed width, as a sprite or tile writer takes them. */
u16 many_args(u8 layer, u16 x, u16 y, u8 tile, u8 palette, const u8 *attr, u16 w, u16 h, u8 flags)
{
    u16 r = (u16)(x + y * 32u);
    r = (u16)(r ^ (u16)(tile << 4) ^ (u16)(palette << 10) ^ (u16)(layer << 13));
    r = (u16)(r + attr[flags & 3u] + w - h);
    return r;
}

/* Out-parameters: a divmod-like split and a min/max through pointers. */
void split(u16 v, u16 *hi, u16 *lo)
{
    *hi = (u16)(v >> 8);
    *lo = (u16)(v & 0xffu);
}

void minmax(const s16 *v, u16 n, s16 *lo, s16 *hi)
{
    u16 i;
    *lo = v[0];
    *hi = v[0];
    for (i = 1; i < n; i++) {
        if (v[i] < *lo) *lo = v[i];
        if (v[i] > *hi) *hi = v[i];
    }
}

/* Function pointers: a hook table like the generated project hooks. */
typedef u16 (*Hook)(u16 value);
static u16 hook_inc(u16 v) { return (u16)(v + 1u); }
static u16 hook_dbl(u16 v) { return (u16)(v << 1); }
static u16 hook_neg(u16 v) { return (u16)(0u - v); }
static u16 hook_swap(u16 v) { return (u16)((v << 8) | (v >> 8)); }
static const Hook hooks[4] = {hook_inc, hook_dbl, hook_neg, hook_swap};

u16 run_hooks(const u8 *program, u16 n, u16 value)
{
    u16 i;
    for (i = 0; i < n; i++)
        value = hooks[program[i] & 3u](value);
    return value;
}
