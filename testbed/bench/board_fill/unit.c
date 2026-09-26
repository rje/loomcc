/* The C that board.asm's loom_pvs_board_fill replaced: Loom
 * cb5a47f^:runtime/src/board.c:292-308 (loom_board_clear: the hash
 * invalidation of loom_board_touch, 38-46, every cell set to 0, then
 * loom_board_redraw_rows over every row, 169-212) with loom_board_cell
 * (163-167) and, from runtime/src/surface.c at the same revision,
 * loom_surface_row (318-326), loom_surface_mark (328-343), loom_surface_cell
 * (196-202) and loom_surface_dirty (169-194).
 *
 * Cut out as a function with the assembly routine's name and interface:
 * the board and its surface arrive as the LoomBoardPaint record board.c
 * fills, so a surface lookup becomes the record's pointers, and the kind
 * the cells become is the argument (loom_board_clear passes 0). Dropped:
 * the checks that need the scene (loom_board_spec, loom_surface_locate's
 * handle and row bounds), which board.c makes before it calls the
 * assembly.
 *
 * Behaviour: the C redraws and marks every row whole; the assembly redraws
 * and marks only the cells that change. The two leave the same shadow,
 * spans and pending count when every cell changes (or its row's span is
 * already whole), which is what the driver sets up. */
#include "loom_board.h"

#define LOOM_SURFACE_CLEAN ((loom_u8)0xffu)

/* surface.c: widens row y's span to cover cells x .. x + count - 1. */
static void loom_surface_dirty(const LoomBoardPaint *spec, loom_u8 y, loom_u8 x,
                               loom_u8 count)
{
    loom_u8 end;

    end = (loom_u8)(x + count - 1u);
    if (spec->first[y] == LOOM_SURFACE_CLEAN) {
        spec->first[y] = x;
        spec->last[y] = end;
        *spec->pending = (loom_u16)(*spec->pending + count);
        return;
    }
    if (x < spec->first[y]) {
        *spec->pending = (loom_u16)(*spec->pending + (loom_u16)(spec->first[y] - x));
        spec->first[y] = x;
    }
    if (end > spec->last[y]) {
        *spec->pending = (loom_u16)(*spec->pending + (loom_u16)(end - spec->last[y]));
        spec->last[y] = end;
    }
}

/* surface.c: loom_surface_row and loom_surface_cell. */
static loom_u16 *loom_surface_row(const LoomBoardPaint *spec, loom_u8 y)
{
    return &spec->shadow[(loom_u16)((loom_u16)y * (loom_u16)spec->stride)];
}

static LoomStatus loom_surface_mark(const LoomBoardPaint *spec, loom_u8 x, loom_u8 y,
                                    loom_u8 count)
{
    if (count == 0u || count > (loom_u8)(spec->stride - x)) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    loom_surface_dirty(spec, y, x, count);
    return LOOM_STATUS_OK;
}

static loom_u8 *loom_board_cell(const LoomBoardPaint *spec, loom_u8 x, loom_u8 y)
{
    return &spec->cells[(loom_u16)((loom_u16)((loom_u16)y * (loom_u16)spec->width) + x)];
}

/* Rewrites the surface words of rows first .. last through the row path
 * and marks them: one mark per tile row, no per-cell calls. */
static LoomStatus loom_board_redraw_rows(const LoomBoardPaint *spec, loom_u8 first,
                                         loom_u8 last)
{
    LoomStatus status;
    loom_u16 *row;
    const loom_u16 *words;
    loom_u8 y;
    loom_u8 sub;
    loom_u8 x;
    loom_u8 column;
    loom_u8 kind;
    loom_u8 span;
    loom_u8 tile_row;

    span = (loom_u8)(spec->width * spec->cell_size);
    for (y = first; y <= last; ++y) {
        for (sub = 0u; sub < spec->cell_size; ++sub) {
            tile_row = (loom_u8)(y * spec->cell_size + sub);
            row = loom_surface_row(spec, tile_row);
            if (row == (loom_u16 *)0) {
                return LOOM_STATUS_INVALID_HANDLE;
            }
            for (x = 0u; x < spec->width; ++x) {
                kind = *loom_board_cell(spec, x, y);
                words = &spec->kind_words[(loom_u16)(
                    (loom_u16)kind * (loom_u16)spec->cell_size * spec->cell_size +
                    (loom_u16)sub * spec->cell_size)];
                for (column = 0u; column < spec->cell_size; ++column) {
                    row[(loom_u16)((loom_u16)x * spec->cell_size + column)] = words[column];
                }
            }
            status = loom_surface_mark(spec, 0u, tile_row, span);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
        if (y == 255u) {
            break;
        }
    }
    return LOOM_STATUS_OK;
}

void loom_pvs_board_fill(const LoomBoardPaint *spec, loom_u16 kind)
{
    loom_u16 cells;
    loom_u16 cell;

    ((LoomBoardPaint *)spec)->hash_valid = 0u;
    cells = (loom_u16)((loom_u16)spec->width * (loom_u16)spec->height);
    for (cell = 0u; cell < cells; ++cell) {
        spec->cells[cell] = (loom_u8)kind;
    }
    (void)loom_board_redraw_rows(spec, 0u, (loom_u8)(spec->height - 1u));
}
