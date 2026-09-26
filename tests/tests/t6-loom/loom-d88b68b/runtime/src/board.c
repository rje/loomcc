#include <loom/board.h>

#define LOOM_BOARD_DIRECTION_COUNT ((loom_u8)4u)

typedef struct LoomBoardState {
    const LoomBoardScene *scene;
    /* Ticks each direction of each board's pad has been held; 0 released. */
    loom_u16 held_ticks[LOOM_BOARD_CAPACITY][LOOM_BOARD_DIRECTION_COUNT];
    /* Ticks until a held direction repeats, once its delay has passed: a
     * countdown rather than a modulo, which is a divide call on 816-tcc. */
    loom_u8 repeat_left[LOOM_BOARD_CAPACITY][LOOM_BOARD_DIRECTION_COUNT];
    loom_u16 moves[LOOM_BOARD_CAPACITY];
    loom_u16 pressed[LOOM_BOARD_CAPACITY];
    loom_u16 held[LOOM_BOARD_CAPACITY];
    loom_u8 initialized;
} LoomBoardState;

static LoomBoardState loom_board_state;
/* Exported, not static: a ROM test can read a board's cells by symbol. */
loom_u8 loom_board_cells[LOOM_BOARD_CELL_CAPACITY];
/* The debug witness asks for every board's hash every tick; it is kept
 * until a cell changes. */
static loom_u16 loom_board_hash_cache[LOOM_BOARD_CAPACITY];
static loom_u8 loom_board_hash_valid[LOOM_BOARD_CAPACITY];

#if defined(LOOM_BOARD_FAST)
LOOM_STATIC_ASSERT(loom_board_paint_matches_board_asm,
                   sizeof(LoomBoardPaint) == LOOM_BOARD_PAINT_BYTES);
/* Bound on a board's first paint of a scene: the surface may activate
 * after the board does. */
static LoomBoardPaint loom_board_paint[LOOM_BOARD_CAPACITY];
static loom_u8 loom_board_paint_bound[LOOM_BOARD_CAPACITY];
#endif

/* Every cell write goes through here or through a whole-board pass that
 * calls it once: the cached hash is stale, and so is the paint binding
 * after a scene change. */
static void loom_board_touch(loom_u8 board)
{
    if (board < LOOM_BOARD_CAPACITY) {
        loom_board_hash_valid[board] = LOOM_FALSE;
#if defined(LOOM_BOARD_FAST)
        loom_board_paint[board].hash_valid = 0u;
#endif
    }
}

static void loom_board_touch_all(void)
{
    loom_u8 board;

    for (board = 0u; board < LOOM_BOARD_CAPACITY; ++board) {
        loom_board_hash_valid[board] = LOOM_FALSE;
#if defined(LOOM_BOARD_FAST)
        loom_board_paint_bound[board] = LOOM_FALSE;
#endif
    }
}

static const loom_u16 loom_board_direction_bits[LOOM_BOARD_DIRECTION_COUNT] = {
    LOOM_BUTTON_LEFT, LOOM_BUTTON_RIGHT, LOOM_BUTTON_DOWN, LOOM_BUTTON_UP};

#if defined(LOOM_BOARD_FAST)
/* The board's record for board.asm, bound on its first use in a scene. */
static const LoomBoardPaint *loom_board_bind(loom_u8 board, const LoomBoardSpec *spec)
{
    LoomBoardPaint *paint;

    paint = &loom_board_paint[board];
    if (loom_board_paint_bound[board] == LOOM_FALSE) {
        if (loom_surface_paint_binding(spec->surface, &paint->shadow, &paint->first,
                                       &paint->last, &paint->pending,
                                       &paint->stride) != LOOM_STATUS_OK) {
            return (const LoomBoardPaint *)0;
        }
        paint->cells = &loom_board_cells[spec->cell_offset];
        paint->kind_words = spec->kind_words;
        paint->width = spec->width;
        paint->height = spec->height;
        paint->cell_size = spec->cell_size;
        paint->hash_valid = 0u;
        loom_board_paint_bound[board] = LOOM_TRUE;
    }
    return paint;
}
#endif

static void loom_board_reset_repeat(void)
{
    loom_u8 board;
    loom_u8 direction;

    for (board = 0u; board < LOOM_BOARD_CAPACITY; ++board) {
        for (direction = 0u; direction < LOOM_BOARD_DIRECTION_COUNT; ++direction) {
            loom_board_state.held_ticks[board][direction] = 0u;
        }
        loom_board_state.moves[board] = 0u;
        loom_board_state.pressed[board] = 0u;
        loom_board_state.held[board] = 0u;
    }
}

LoomStatus loom_board_initialize(void)
{
    loom_board_state.scene = (const LoomBoardScene *)0;
    loom_board_state.initialized = LOOM_TRUE;
    loom_board_reset_repeat();
    loom_board_touch_all();
    return LOOM_STATUS_OK;
}

static LoomStatus loom_board_validate(const LoomBoardScene *scene)
{
    const LoomBoardSpec *spec;
    loom_u8 index;
    loom_u16 cells;

    if (scene->board_count > LOOM_BOARD_CAPACITY) {
        return LOOM_STATUS_CAPACITY;
    }
    if (scene->board_count != 0u && scene->boards == (const LoomBoardSpec *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < scene->board_count; ++index) {
        spec = &scene->boards[index];
        if (spec->width == 0u || spec->width > LOOM_BOARD_WIDTH_MAX ||
            spec->height == 0u || spec->height > LOOM_BOARD_HEIGHT_MAX ||
            (spec->cell_size != 1u && spec->cell_size != 2u) ||
            spec->kind_count == 0u || spec->kind_count > LOOM_BOARD_KIND_CAPACITY ||
            spec->pad == 0u || spec->pad > LOOM_INPUT_PAD_CAPACITY ||
            spec->das_period_ticks == 0u ||
            spec->kind_words == (const loom_u16 *)0) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
        cells = (loom_u16)((loom_u16)spec->width * (loom_u16)spec->height);
        if (spec->cell_offset > LOOM_BOARD_CELL_CAPACITY ||
            cells > (loom_u16)(LOOM_BOARD_CELL_CAPACITY - spec->cell_offset)) {
            return LOOM_STATUS_CAPACITY;
        }
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_board_activate_scene(const LoomBoardScene *scene)
{
    const LoomBoardSpec *spec;
    LoomStatus status;
    loom_u8 index;
    loom_u16 cells;
    loom_u16 cell;

    if (loom_board_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    loom_board_reset_repeat();
    loom_board_touch_all();
    if (scene == (const LoomBoardScene *)0) {
        loom_board_state.scene = scene;
        return LOOM_STATUS_OK;
    }
    status = loom_board_validate(scene);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    for (index = 0u; index < scene->board_count; ++index) {
        spec = &scene->boards[index];
        cells = (loom_u16)((loom_u16)spec->width * (loom_u16)spec->height);
        for (cell = 0u; cell < cells; ++cell) {
            loom_board_cells[spec->cell_offset + cell] = 0u;
        }
    }
    loom_board_state.scene = scene;
    return LOOM_STATUS_OK;
}

static const LoomBoardSpec *loom_board_spec(loom_u8 board)
{
    const LoomBoardScene *scene;

    scene = loom_board_state.scene;
    if (loom_board_state.initialized == LOOM_FALSE ||
        scene == (const LoomBoardScene *)0 || board >= scene->board_count) {
        return (const LoomBoardSpec *)0;
    }
    return &scene->boards[board];
}

static loom_u8 *loom_board_cell(const LoomBoardSpec *spec, loom_u8 x, loom_u8 y)
{
    return &loom_board_cells[(loom_u16)(
        spec->cell_offset + (loom_u16)((loom_u16)y * (loom_u16)spec->width) + x)];
}

/* Rewrites the surface words of rows first .. last through the row path
 * and marks them: one mark per tile row, no per-cell calls. */
static LoomStatus loom_board_redraw_rows(const LoomBoardSpec *spec, loom_u8 first,
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
            row = loom_surface_row(spec->surface, tile_row);
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
            status = loom_surface_mark(spec->surface, 0u, tile_row, span);
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

static LoomStatus loom_board_draw_cell(const LoomBoardSpec *spec, loom_u8 x, loom_u8 y)
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
        row = loom_surface_row(spec->surface, (loom_u8)(y * spec->cell_size + sub));
        if (row == (loom_u16 *)0) {
            return LOOM_STATUS_INVALID_HANDLE;
        }
        words = &spec->kind_words[(loom_u16)(
            (loom_u16)kind * (loom_u16)spec->cell_size * spec->cell_size +
            (loom_u16)sub * spec->cell_size)];
        for (column = 0u; column < spec->cell_size; ++column) {
            row[(loom_u16)(tile_x + column)] = words[column];
        }
        status = loom_surface_mark(spec->surface, tile_x,
                                   (loom_u8)(y * spec->cell_size + sub), spec->cell_size);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
    }
    return LOOM_STATUS_OK;
}

loom_u8 loom_board_width(loom_u8 board)
{
    const LoomBoardSpec *spec;

    spec = loom_board_spec(board);
    return spec == (const LoomBoardSpec *)0 ? 0u : spec->width;
}

loom_u8 loom_board_height(loom_u8 board)
{
    const LoomBoardSpec *spec;

    spec = loom_board_spec(board);
    return spec == (const LoomBoardSpec *)0 ? 0u : spec->height;
}

loom_u8 loom_board_get(loom_u8 board, loom_u8 x, loom_u8 y)
{
    const LoomBoardSpec *spec;

    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0 || x >= spec->width || y >= spec->height) {
        return 0u;
    }
    return *loom_board_cell(spec, x, y);
}

LoomStatus loom_board_set(loom_u8 board, loom_u8 x, loom_u8 y, loom_u8 kind)
{
    const LoomBoardSpec *spec;

    loom_board_touch(board);
    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (x >= spec->width || y >= spec->height || kind >= spec->kind_count) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
    if (*loom_board_cell(spec, x, y) == kind) {
        return LOOM_STATUS_OK;
    }
    *loom_board_cell(spec, x, y) = kind;
    return loom_board_draw_cell(spec, x, y);
}

LoomStatus loom_board_clear(loom_u8 board)
{
    const LoomBoardSpec *spec;
    loom_u16 cells;
    loom_u16 cell;

    loom_board_touch(board);
    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
#if defined(LOOM_BOARD_FAST)
    {
        const LoomBoardPaint *paint;

        paint = loom_board_bind(board, spec);
        if (paint == (const LoomBoardPaint *)0) {
            return LOOM_STATUS_INVALID_HANDLE;
        }
        (void)cells;
        (void)cell;
        loom_pvs_board_fill(paint, 0u);
        return LOOM_STATUS_OK;
    }
#else
    cells = (loom_u16)((loom_u16)spec->width * (loom_u16)spec->height);
    for (cell = 0u; cell < cells; ++cell) {
        loom_board_cells[spec->cell_offset + cell] = 0u;
    }
    return loom_board_redraw_rows(spec, 0u, (loom_u8)(spec->height - 1u));
#endif
}

loom_u8 loom_board_fits(loom_u8 board, loom_u16 shape, loom_s8 x, loom_s8 y)
{
    const LoomBoardSpec *spec;
    loom_u8 row;
    loom_u8 column;
    loom_s16 cell_x;
    loom_s16 cell_y;

    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0) {
        return LOOM_FALSE;
    }
#if defined(LOOM_BOARD_FAST)
    {
        const LoomBoardPaint *paint;

        paint = loom_board_bind(board, spec);
        if (paint == (const LoomBoardPaint *)0) {
            return LOOM_FALSE;
        }
        return (loom_u8)loom_pvs_board_fits(paint, shape, (loom_s16)x, (loom_s16)y);
    }
#endif
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

static LoomStatus loom_board_paint_shape(loom_u8 board, loom_u16 shape, loom_s8 x,
                                         loom_s8 y, loom_u8 kind)
{
    const LoomBoardSpec *spec;
    LoomStatus status;
    loom_u8 row;
    loom_u8 column;
    loom_s16 cell_x;
    loom_s16 cell_y;

    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    if (kind >= spec->kind_count) {
        return LOOM_STATUS_OUT_OF_RANGE;
    }
#if defined(LOOM_BOARD_FAST)
    {
        const LoomBoardPaint *paint;

        paint = loom_board_bind(board, spec);
        if (paint == (const LoomBoardPaint *)0) {
            return LOOM_STATUS_INVALID_HANDLE;
        }
        loom_pvs_board_paint(paint, shape, (loom_s16)x, (loom_s16)y, kind);
        return LOOM_STATUS_OK;
    }
#endif
    loom_board_touch(board);
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
            status = loom_board_set(board, (loom_u8)cell_x, (loom_u8)cell_y, kind);
            if (status != LOOM_STATUS_OK) {
                return status;
            }
        }
    }
    return LOOM_STATUS_OK;
}

LoomStatus loom_board_stamp(loom_u8 board, loom_u16 shape, loom_s8 x, loom_s8 y,
                            loom_u8 kind)
{
    return loom_board_paint_shape(board, shape, x, y, kind);
}

LoomStatus loom_board_erase(loom_u8 board, loom_u16 shape, loom_s8 x, loom_s8 y)
{
    return loom_board_paint_shape(board, shape, x, y, 0u);
}

loom_u8 loom_board_move(loom_u8 board, loom_u16 from_shape, loom_s8 from_x,
                        loom_s8 from_y, loom_u16 to_shape, loom_s8 to_x,
                        loom_s8 to_y, loom_u8 kind)
{
    const LoomBoardSpec *spec;

    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0 || kind >= spec->kind_count) {
        return LOOM_FALSE;
    }
#if defined(LOOM_BOARD_FAST)
    {
        const LoomBoardPaint *paint;

        paint = loom_board_bind(board, spec);
        if (paint == (const LoomBoardPaint *)0) {
            return LOOM_FALSE;
        }
        return (loom_u8)loom_pvs_board_move(paint, from_shape, (loom_s16)from_x,
                                            (loom_s16)from_y, to_shape,
                                            (loom_s16)to_x, (loom_s16)to_y, kind);
    }
#else
    {
        loom_u8 fits;

        (void)loom_board_erase(board, from_shape, from_x, from_y);
        fits = loom_board_fits(board, to_shape, to_x, to_y);
        if (fits != LOOM_FALSE) {
            (void)loom_board_stamp(board, to_shape, to_x, to_y, kind);
        } else {
            (void)loom_board_stamp(board, from_shape, from_x, from_y, kind);
        }
        return fits;
    }
#endif
}

loom_u8 loom_board_drop(loom_u8 board, loom_u16 shape, loom_s8 x, loom_s8 y,
                        loom_u8 kind)
{
    const LoomBoardSpec *spec;

    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0 || kind >= spec->kind_count) {
        return 0u;
    }
#if defined(LOOM_BOARD_FAST)
    {
        const LoomBoardPaint *paint;

        paint = loom_board_bind(board, spec);
        if (paint == (const LoomBoardPaint *)0) {
            return 0u;
        }
        return (loom_u8)loom_pvs_board_drop(paint, shape, (loom_s16)x, (loom_s16)y,
                                            kind);
    }
#else
    {
        loom_u8 rows;

        (void)loom_board_erase(board, shape, x, y);
        rows = 0u;
        while (loom_board_fits(board, shape, x, (loom_s8)(y + 1)) != LOOM_FALSE) {
            ++y;
            ++rows;
        }
        (void)loom_board_stamp(board, shape, x, y, kind);
        return rows;
    }
#endif
}

loom_u8 loom_board_clear_full_rows(loom_u8 board)
{
    const LoomBoardSpec *spec;
    loom_u8 cleared;
    loom_u8 y;
    loom_u8 x;
    loom_u8 full;
    loom_u8 write;
    loom_u8 lowest;
    loom_s16 read;

    loom_board_touch(board);
    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0) {
        return 0u;
    }
#if defined(LOOM_BOARD_FAST)
    {
        const LoomBoardPaint *paint;

        paint = loom_board_bind(board, spec);
        if (paint == (const LoomBoardPaint *)0) {
            return 0u;
        }
        (void)cleared;
        (void)y;
        (void)x;
        (void)full;
        (void)write;
        (void)lowest;
        (void)read;
        return (loom_u8)loom_pvs_board_clear_full_rows(paint);
    }
#endif
    cleared = 0u;
    lowest = 0u;
    /* Compact from the bottom: write keeps rows that are not full. */
    write = (loom_u8)(spec->height - 1u);
    for (read = (loom_s16)(spec->height - 1); read >= 0; --read) {
        full = LOOM_TRUE;
        for (x = 0u; x < spec->width; ++x) {
            if (*loom_board_cell(spec, x, (loom_u8)read) == 0u) {
                full = LOOM_FALSE;
                break;
            }
        }
        if (full != LOOM_FALSE) {
            if (cleared == 0u) {
                lowest = (loom_u8)read;
            }
            ++cleared;
            continue;
        }
        if (write != (loom_u8)read) {
            for (x = 0u; x < spec->width; ++x) {
                *loom_board_cell(spec, x, write) = *loom_board_cell(spec, x, (loom_u8)read);
            }
        }
        --write;
        if (write == 255u) {
            break;
        }
    }
    if (cleared == 0u) {
        return 0u;
    }
    /* The rows that dropped in from above are now empty. */
    for (y = 0u; y < cleared; ++y) {
        for (x = 0u; x < spec->width; ++x) {
            *loom_board_cell(spec, x, y) = 0u;
        }
    }
    (void)loom_board_redraw_rows(spec, 0u, lowest);
    return cleared;
}

loom_u16 loom_board_clear_runs(loom_u8 board, loom_u8 min_run)
{
    const LoomBoardSpec *spec;
    loom_u16 cleared;
    loom_u8 x;
    loom_u8 y;
    loom_u8 run;
    loom_u8 kind;
    loom_u8 back;
    loom_u8 first_row;
    loom_u8 last_row;
    /* Cells to clear are marked in the high bit and cleared together, so
     * a cell shared by a horizontal and a vertical run counts once. */
    const loom_u8 mark = 0x80u;

    loom_board_touch(board);
    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0 || min_run < 2u) {
        return 0u;
    }
    for (y = 0u; y < spec->height; ++y) {
        run = 0u;
        kind = 0u;
        for (x = 0u; x <= spec->width; ++x) {
            loom_u8 here;

            here = x < spec->width ? (loom_u8)(*loom_board_cell(spec, x, y) & 0x7fu) : 0u;
            if (here != 0u && here == kind) {
                ++run;
                continue;
            }
            if (kind != 0u && run >= min_run) {
                for (back = 1u; back <= run; ++back) {
                    *loom_board_cell(spec, (loom_u8)(x - back), y) |= mark;
                }
            }
            kind = here;
            run = 1u;
        }
    }
    for (x = 0u; x < spec->width; ++x) {
        run = 0u;
        kind = 0u;
        for (y = 0u; y <= spec->height; ++y) {
            loom_u8 here;

            here = y < spec->height ? (loom_u8)(*loom_board_cell(spec, x, y) & 0x7fu) : 0u;
            if (here != 0u && here == kind) {
                ++run;
                continue;
            }
            if (kind != 0u && run >= min_run) {
                for (back = 1u; back <= run; ++back) {
                    *loom_board_cell(spec, x, (loom_u8)(y - back)) |= mark;
                }
            }
            kind = here;
            run = 1u;
        }
    }
    cleared = 0u;
    first_row = 255u;
    last_row = 0u;
    for (y = 0u; y < spec->height; ++y) {
        for (x = 0u; x < spec->width; ++x) {
            if ((*loom_board_cell(spec, x, y) & mark) != 0u) {
                *loom_board_cell(spec, x, y) = 0u;
                ++cleared;
                if (first_row == 255u) {
                    first_row = y;
                }
                last_row = y;
            }
        }
    }
    if (cleared != 0u) {
        (void)loom_board_redraw_rows(spec, first_row, last_row);
    }
    return cleared;
}

loom_u16 loom_board_settle(loom_u8 board)
{
    const LoomBoardSpec *spec;
    loom_u16 moved;
    loom_u8 x;
    loom_s16 read;
    loom_u8 write;
    loom_u8 kind;
    loom_u8 first_row;

    loom_board_touch(board);
    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0) {
        return 0u;
    }
    moved = 0u;
    first_row = 255u;
    for (x = 0u; x < spec->width; ++x) {
        write = (loom_u8)(spec->height - 1u);
        for (read = (loom_s16)(spec->height - 1); read >= 0; --read) {
            kind = *loom_board_cell(spec, x, (loom_u8)read);
            if (kind == 0u) {
                continue;
            }
            if (write != (loom_u8)read) {
                *loom_board_cell(spec, x, write) = kind;
                *loom_board_cell(spec, x, (loom_u8)read) = 0u;
                ++moved;
                if (first_row == 255u || (loom_u8)read < first_row) {
                    first_row = (loom_u8)read;
                }
            }
            --write;
        }
    }
    if (moved != 0u) {
        (void)loom_board_redraw_rows(spec, first_row, (loom_u8)(spec->height - 1u));
    }
    return moved;
}

loom_u16 loom_board_hash(loom_u8 board)
{
    const LoomBoardSpec *spec;
    loom_u16 hash;
    loom_u16 cells;
    loom_u16 cell;

    spec = loom_board_spec(board);
    if (spec == (const LoomBoardSpec *)0) {
        return 0u;
    }
    cells = (loom_u16)((loom_u16)spec->width * (loom_u16)spec->height);
#if !defined(LOOM_BOARD_FAST)
    if (loom_board_hash_valid[board] != LOOM_FALSE) {
        return loom_board_hash_cache[board];
    }
#endif
#if defined(LOOM_BOARD_FAST)
    {
        LoomBoardPaint *paint;

        paint = (LoomBoardPaint *)loom_board_bind(board, spec);
        if (paint != (LoomBoardPaint *)0 && paint->hash_valid != 0u) {
            return paint->hash;
        }
        hash = loom_pvs_board_hash(&loom_board_cells[spec->cell_offset], cells);
        if (paint != (LoomBoardPaint *)0) {
            paint->hash = hash;
            paint->hash_valid = 1u;
        }
        return hash;
    }
#else
    /* Each cell weighted by its odd position, so one write moves the hash
     * by (new - old) * (2 * index + 1) and board.asm keeps it as it paints. */
    hash = 0u;
    for (cell = 0u; cell < cells; ++cell) {
        hash = (loom_u16)(hash + (loom_u16)(loom_board_cells[spec->cell_offset + cell] *
                                            (loom_u16)(2u * cell + 1u)));
    }
    loom_board_hash_cache[board] = hash;
    loom_board_hash_valid[board] = LOOM_TRUE;
    return hash;
#endif
}

LoomStatus loom_board_repeat_update(const LoomInputSnapshot *input)
{
    const LoomBoardScene *scene;
    const LoomBoardSpec *spec;
    loom_u8 board;
    loom_u8 direction;
    loom_u16 held;
    loom_u16 fired;
    loom_u16 ticks;

    if (loom_board_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (input == (const LoomInputSnapshot *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    scene = loom_board_state.scene;
    if (scene == (const LoomBoardScene *)0) {
        return LOOM_STATUS_OK;
    }
    /* Pointer walks throughout: an indexed struct or a two-dimensional
     * subscript is a multiply call on 816-tcc. */
    spec = scene->boards;
    for (board = 0u; board < scene->board_count; ++board, ++spec) {
        const LoomPadFrame *pad;
        loom_u16 *held_ticks;
        loom_u8 *repeat_left;
        loom_u16 pressed;
        loom_u16 fire_at;

        pad = (const LoomPadFrame *)0;
        if (spec->pad <= input->pad_count) {
            pad = &input->pads[0];
            if (spec->pad == 2u) {
                ++pad;
            }
        }
        held = pad != (const LoomPadFrame *)0 ? pad->held : 0u;
        pressed = pad != (const LoomPadFrame *)0 ? pad->pressed : 0u;
        held_ticks = loom_board_state.held_ticks[board];
        repeat_left = loom_board_state.repeat_left[board];
        /* A held direction fires on its first tick, again once the delay
         * has passed (tick delay + 2), then every period after that. */
#if defined(LOOM_BOARD_FAST)
        (void)fire_at;
        (void)direction;
        (void)ticks;
        fired = loom_pvs_board_repeat(held_ticks, repeat_left, held,
                                      spec->das_delay_ticks, spec->das_period_ticks);
#else
        fire_at = (loom_u16)(spec->das_delay_ticks + 2u);
        fired = 0u;
        for (direction = 0u; direction < LOOM_BOARD_DIRECTION_COUNT;
             ++direction, ++held_ticks, ++repeat_left) {
            loom_u16 bit;

            bit = loom_board_direction_bits[direction];
            if ((held & bit) == 0u) {
                *held_ticks = 0u;
                continue;
            }
            ticks = *held_ticks;
            if (ticks != 0xffffu) {
                ++ticks;
            }
            *held_ticks = ticks;
            if (ticks == 1u) {
                fired |= bit;
            } else if (ticks == fire_at) {
                fired |= bit;
                *repeat_left = spec->das_period_ticks;
            } else if (ticks > fire_at) {
                --*repeat_left;
                if (*repeat_left == 0u) {
                    fired |= bit;
                    *repeat_left = spec->das_period_ticks;
                }
            }
        }
#endif
        loom_board_state.moves[board] = fired;
        loom_board_state.pressed[board] = pressed;
        loom_board_state.held[board] = held;
    }
    return LOOM_STATUS_OK;
}

loom_u16 loom_board_moves(loom_u8 board)
{
    if (loom_board_spec(board) == (const LoomBoardSpec *)0) {
        return 0u;
    }
    return loom_board_state.moves[board];
}

loom_u16 loom_board_pressed(loom_u8 board)
{
    if (loom_board_spec(board) == (const LoomBoardSpec *)0) {
        return 0u;
    }
    return loom_board_state.pressed[board];
}

loom_u16 loom_board_held(loom_u8 board)
{
    if (loom_board_spec(board) == (const LoomBoardSpec *)0) {
        return 0u;
    }
    return loom_board_state.held[board];
}
