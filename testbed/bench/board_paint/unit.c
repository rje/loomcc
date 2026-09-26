/* The C that board.asm's loom_pvs_board_paint replaced: Loom
 * e239e1c^:runtime/src/board.c:298-333 (loom_board_paint_shape, the body of
 * loom_board_stamp / loom_board_erase), with what it called for every cell
 * -- loom_board_set (229-245), loom_board_draw_cell (170-200) and
 * loom_board_cell (118-122) -- and, from runtime/src/surface.c at the same
 * revision, loom_surface_row (287-295), loom_surface_mark (297-312),
 * loom_surface_cell (186-192) and loom_surface_dirty (159-184).
 *
 * Cut out as one function with the assembly routine's name and interface:
 * the board and its surface arrive as the LoomBoardPaint record board.c
 * fills (cells, kind words, the surface's shadow, dirty-span bytes, pending
 * count and stride), so a surface lookup (loom_surface_locate) becomes the
 * record's pointers. Dropped with it: the checks that need the scene
 * (loom_board_spec, loom_surface_locate's handle and row bounds, the kind
 * against kind_count), which board.c makes before it calls the assembly.
 * Added: the kept hash (hash += (kind - old) * (2 * index + 1) while
 * hash_valid), which the commit introduced with the assembly. */
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

static LoomStatus loom_board_draw_cell(const LoomBoardPaint *spec, loom_u8 x, loom_u8 y)
{
    LoomStatus status;
    loom_u16 *row;
    const loom_u16 *words;
    loom_u8 sub;
    loom_u8 column;
    loom_u8 kind;
    loom_u8 tile_x;

    kind = *loom_board_cell(spec, x, y);
    tile_x = (loom_u8)(x * spec->cell_size);
    for (sub = 0u; sub < spec->cell_size; ++sub) {
        row = loom_surface_row(spec, (loom_u8)(y * spec->cell_size + sub));
        if (row == (loom_u16 *)0) {
            return LOOM_STATUS_INVALID_HANDLE;
        }
        words = &spec->kind_words[(loom_u16)(
            (loom_u16)kind * (loom_u16)spec->cell_size * spec->cell_size +
            (loom_u16)sub * spec->cell_size)];
        for (column = 0u; column < spec->cell_size; ++column) {
            row[(loom_u16)(tile_x + column)] = words[column];
        }
        status = loom_surface_mark(spec, tile_x, (loom_u8)(y * spec->cell_size + sub),
                                   (loom_u8)spec->cell_size);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    return LOOM_STATUS_OK;
}

static LoomStatus loom_board_set(const LoomBoardPaint *spec, loom_u8 x, loom_u8 y,
                                 loom_u8 kind)
{
    loom_u8 old;

    if (x >= spec->width || y >= spec->height) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    old = *loom_board_cell(spec, x, y);
    if (old == kind) {
        return LOOM_STATUS_OK;
    }
    *loom_board_cell(spec, x, y) = kind;
    if (spec->hash_valid != 0u) {
        ((LoomBoardPaint *)spec)->hash = (loom_u16)(
            spec->hash +
            (loom_u16)((loom_u16)(kind - old) *
                       (loom_u16)(2u * (loom_u16)((loom_u16)((loom_u16)y * spec->width) + x) +
                                  1u)));
    }
    return loom_board_draw_cell(spec, x, y);
}

void loom_pvs_board_paint(const LoomBoardPaint *spec, loom_u16 shape, loom_s16 x,
                          loom_s16 y, loom_u16 kind16)
{
    LoomStatus status;
    loom_u8 kind;
    loom_u8 row;
    loom_u8 column;
    loom_s16 cell_x;
    loom_s16 cell_y;

    kind = (loom_u8)kind16;
    for (row = 0u; row < LOOM_BOARD_SHAPE_SIZE; ++row) {
        for (column = 0u; column < LOOM_BOARD_SHAPE_SIZE; ++column) {
            if ((shape & (loom_u16)(1u << (row * LOOM_BOARD_SHAPE_SIZE + column))) == 0u) {
                continue;
            }
            cell_x = (loom_s16)(x + column);
            cell_y = (loom_s16)(y + row);
            if (cell_x < 0 || cell_x >= (loom_s16)spec->width || cell_y < 0 ||
                cell_y >= (loom_s16)spec->height) {
                continue;
            }
            status = loom_board_set(spec, (loom_u8)cell_x, (loom_u8)cell_y, kind);
            if (status != LOOM_STATUS_OK) {
                return;
            }
        }
    }
}
