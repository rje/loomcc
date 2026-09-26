/* The C that board.asm's loom_pvs_board_fits replaced (Loom
 * e239e1c^:runtime/src/board.c:264-296, loom_board_fits, with its cell
 * lookup loom_board_cell from lines 118-122), cut out as a function with the
 * assembly routine's name and interface: the board arrives as the
 * LoomBoardPaint record board.c fills (its `cells` is
 * &loom_board_cells[spec->cell_offset], its width and height the spec's),
 * and x, y arrive widened to 16 bits as board.c passes them. */
#include "loom_board.h"

static loom_u8 *loom_board_cell(const LoomBoardPaint *spec, loom_u8 x, loom_u8 y)
{
    return &spec->cells[(loom_u16)((loom_u16)((loom_u16)y * (loom_u16)spec->width) + x)];
}

loom_u16 loom_pvs_board_fits(const LoomBoardPaint *spec, loom_u16 shape, loom_s16 x,
                             loom_s16 y)
{
    loom_u8 row;
    loom_u8 column;
    loom_s16 cell_x;
    loom_s16 cell_y;

    for (row = 0u; row < LOOM_BOARD_SHAPE_SIZE; ++row) {
        for (column = 0u; column < LOOM_BOARD_SHAPE_SIZE; ++column) {
            if ((shape & (loom_u16)(1u << (row * LOOM_BOARD_SHAPE_SIZE + column))) == 0u) {
                continue;
            }
            cell_x = (loom_s16)(x + column);
            cell_y = (loom_s16)(y + row);
            if (cell_x < 0 || cell_x >= (loom_s16)spec->width ||
                cell_y >= (loom_s16)spec->height) {
                return LOOM_FALSE;
            }
            if (cell_y < 0) {
                continue;
            }
            if (*loom_board_cell(spec, (loom_u8)cell_x, (loom_u8)cell_y) != 0u) {
                return LOOM_FALSE;
            }
        }
    }
    return LOOM_TRUE;
}
