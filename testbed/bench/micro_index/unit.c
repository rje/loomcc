/* Array indexing: global u8 and u16 arrays, a 2-D u8 map indexed [y][x], a
 * 2-D u16 table, a lookup through a const table, and a histogram (an index
 * that is itself loaded from an array). */

#define MAP_W 16
#define MAP_H 12

unsigned char map[MAP_H][MAP_W];
unsigned short cost[8][8];
unsigned short hist[16];
unsigned char line_u8[32];
unsigned short line_u16[32];

static const unsigned char tile_weight[8] = {0, 1, 3, 7, 2, 5, 9, 4};

/* Weighted count of map cells in a rectangle: map[y][x] through a const table. */
unsigned short map_weight(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1)
{
    unsigned short x, y, total = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            total = (unsigned short)(total + tile_weight[map[y][x] & 7u]);
    return total;
}

/* One relaxation sweep over the top-left 6x6 of a 2-D u16 table. */
void cost_sweep(void)
{
    unsigned short i, j;
    for (i = 1; i < 6; i++)
        for (j = 1; j < 6; j++) {
            unsigned short up = cost[i - 1][j];
            unsigned short left = cost[i][j - 1];
            unsigned short best = up < left ? up : left;
            cost[i][j] = (unsigned short)(cost[i][j] + best);
        }
}

/* hist[line_u8[i] >> 4]++: an index loaded from another array. */
void histogram(unsigned short n)
{
    unsigned short i;
    for (i = 0; i < n; i++)
        hist[line_u8[i] >> 4]++;
}

/* Reverse a u16 array in place with two indices. */
void reverse_u16(unsigned short n)
{
    unsigned short i = 0, j = (unsigned short)(n - 1u);
    while (i < j) {
        unsigned short t = line_u16[i];
        line_u16[i] = line_u16[j];
        line_u16[j] = t;
        i++;
        j--;
    }
}
