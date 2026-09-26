/* The test room every body-probe bench shares: 24x14 cells (384x224
 * pixels), a solid border (row 0 solid across, so no ceiling scan ever
 * leaves the room upward), a few solid blocks, one-way ledges ('='),
 * a hill of slopes ('/' up right, '\' up left). Included once, by
 * driver.c, which owns the grid and loom_movement_grid. */
#define GRID_W 24
#define GRID_H 14

static const char *const grid_rows[GRID_H] = {
    "########################",
    "#......................#",
    "#......................#",
    "#....###.......====....#",
    "#......................#",
    "#..........#...........#",
    "#......................#",
    "#.....====........#....#",
    "#.................#....#",
    "#............/#\\..#....#",
    "#.........../####\\.....#",
    "#.........#########....#",
    "#......................#",
    "########################",
};

static loom_u8 grid_cells[GRID_W * GRID_H];
LoomMovementGrid loom_movement_grid;

#if defined(__65816__)
typedef char loom_movement_grid_matches_body_asm
    [sizeof(LoomMovementGrid) == LOOM_MOVEMENT_GRID_BYTES ? 1 : -1];
#endif

static void grid_build(void)
{
    unsigned short x, y;
    loom_u8 cell;
    char c;

    for (y = 0; y < GRID_H; y++) {
        for (x = 0; x < GRID_W; x++) {
            c = grid_rows[y][x];
            cell = LOOM_MOVEMENT_COLLISION_NONE;
            if (c == '#')
                cell = LOOM_MOVEMENT_COLLISION_SOLID;
            else if (c == '=')
                cell = LOOM_MOVEMENT_COLLISION_ONE_WAY;
            else if (c == '/')
                cell = LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT;
            else if (c == '\\')
                cell = LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT;
            grid_cells[y * GRID_W + x] = cell;
        }
    }
    loom_movement_grid.cells = grid_cells;
    loom_movement_grid.pixel_width = GRID_W * 16;
    loom_movement_grid.pixel_height = GRID_H * 16;
    for (y = 0; y < LOOM_MOVEMENT_MAX_COLLISION_ROWS; y++)
        loom_movement_grid.row_offsets[y] = (loom_u16)(y < GRID_H ? y * GRID_W : 0u);
}
