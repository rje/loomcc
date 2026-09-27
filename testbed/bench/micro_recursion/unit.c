/* Recursion as game code meets it: a flood fill (four recursive calls per
 * cell), a recursive-descent expression evaluator (three mutually recursive
 * functions), and a tree walk passing a pointer to the caller's state. */

typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;

#define GW 4
#define GH 3
u8 grid[GH][GW];

u16 fill_rec(u8 x, u8 y, u8 from, u8 to)
{
    u16 n;
    if (x >= GW || y >= GH || grid[y][x] != from)
        return 0;
    grid[y][x] = to;
    n = 1;
    n = (u16)(n + fill_rec((u8)(x + 1), y, from, to));
    n = (u16)(n + fill_rec((u8)(x - 1), y, from, to));
    n = (u16)(n + fill_rec(x, (u8)(y + 1), from, to));
    n = (u16)(n + fill_rec(x, (u8)(y - 1), from, to));
    return n;
}

u16 flood(u8 x, u8 y, u8 to)
{
    u8 from = grid[y][x];
    if (from == to)
        return 0;
    return fill_rec(x, y, from, to);
}

static const u8 *tok;
s16 expr(void);

s16 atom(void)
{
    s16 v;
    u8 t = *tok++;
    if (t == '(') {
        v = expr();
        tok++;
        return v;
    }
    if (t == '-')
        return (s16)-atom();
    return (s16)(t - '0');
}

s16 term(void)
{
    s16 v = atom();
    while (*tok == '*') {
        tok++;
        v = (s16)(v * atom());
    }
    return v;
}

s16 expr(void)
{
    s16 v = term();
    while (*tok == '+' || *tok == '-') {
        u8 op = *tok++;
        s16 r = term();
        v = op == '+' ? (s16)(v + r) : (s16)(v - r);
    }
    return v;
}

s16 eval(const u8 *s)
{
    tok = s;
    return expr();
}

typedef struct {
    s16 value;
    u8 left, right; /* 0xff: none */
} Node;

s16 tree_sum(const Node *nodes, u8 i, u16 depth, u16 *max_depth)
{
    s16 s;
    if (i == 0xff)
        return 0;
    if (depth > *max_depth)
        *max_depth = depth;
    s = nodes[i].value;
    s = (s16)(s + tree_sum(nodes, nodes[i].left, (u16)(depth + 1), max_depth));
    s = (s16)(s + tree_sum(nodes, nodes[i].right, (u16)(depth + 1), max_depth));
    return s;
}
