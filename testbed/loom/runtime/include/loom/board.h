#ifndef LOOM_BOARD_H
#define LOOM_BOARD_H

#include <loom/input.h>
#include <loom/pools.h>
#include <loom/surface.h>

/*
 * Boards (GRID-001): a packed logical grid of cells drawn through a
 * surface. A cell is one byte, its kind; kind 0 is empty and shows the
 * room's painted tiles under the board. Every other kind draws the words
 * its vocabulary gives it, cell_size by cell_size tiles, through the
 * surface's row path. The rules are the game's: its hook calls the helpers
 * here (fits, stamp, erase, full-row clears, run clears, settling) on its
 * own timing, and the board hashes its cells so a ROM test can pin a whole
 * state after a replayed input sequence.
 *
 * Delayed auto-shift: a held direction fires on its first tick, again
 * after the board's delay, then every period, so a piece slides the way
 * puzzle games expect; loom_board_moves reads the board's pad.
 */

#define LOOM_BOARD_WIDTH_MAX ((loom_u8)32u)
#define LOOM_BOARD_HEIGHT_MAX ((loom_u8)32u)
#define LOOM_BOARD_KIND_CAPACITY ((loom_u8)16u)
/* A 4x4 shape mask: bit (row * 4 + column), row 0 the top. */
#define LOOM_BOARD_SHAPE_SIZE ((loom_u8)4u)

typedef struct LoomBoardSpec {
    /* The surface the board draws through, the same geometry in tiles. */
    loom_u8 surface;
    loom_u8 width;
    loom_u8 height;
    /* Tiles a cell spans each way: 1 or 2. */
    loom_u8 cell_size;
    /* Kinds in the vocabulary, kind 0 included. */
    loom_u8 kind_count;
    /* The pad the board listens to: 1 or 2. */
    loom_u8 pad;
    loom_u8 das_delay_ticks;
    loom_u8 das_period_ticks;
    /* Where this board's cells start in the shared cell block. */
    loom_u16 cell_offset;
    /* kind_count * cell_size * cell_size tilemap words in reading order;
     * kind 0's words are the painted tiles' and are written on erase. */
    const loom_u16 *kind_words;
} LoomBoardSpec;

typedef struct LoomBoardScene {
    const LoomBoardSpec *boards;
    loom_u8 board_count;
    loom_u8 reserved;
} LoomBoardScene;

#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
#define LOOM_BOARD_FAST 1
/* What board.asm paints a board through, filled once per scene by board.c;
 * board.asm reads it by offset (32 bytes, asserted in board.c). */
typedef struct LoomBoardPaint {
    loom_u8 *cells;
    const loom_u16 *kind_words;
    loom_u16 *shadow;
    loom_u8 *first;
    loom_u8 *last;
    loom_u16 *pending;
    loom_u16 width;
    loom_u16 height;
    loom_u16 cell_size;
    loom_u16 stride;
    /* The board's hash, kept by every paint while hash_valid is set. */
    loom_u16 hash;
    loom_u16 hash_valid;
} LoomBoardPaint;
#define LOOM_BOARD_PAINT_BYTES 36u
loom_u16 loom_pvs_board_fits(const LoomBoardPaint *paint, loom_u16 shape,
                             loom_s16 x, loom_s16 y);
void loom_pvs_board_paint(const LoomBoardPaint *paint, loom_u16 shape,
                          loom_s16 x, loom_s16 y, loom_u16 kind);
loom_u16 loom_pvs_board_hash(const loom_u8 *cells, loom_u16 count);
loom_u16 loom_pvs_board_clear_full_rows(const LoomBoardPaint *paint);
loom_u16 loom_pvs_board_drop(const LoomBoardPaint *paint, loom_u16 shape,
                             loom_s16 x, loom_s16 y, loom_u16 kind);
void loom_pvs_board_fill(const LoomBoardPaint *paint, loom_u16 kind);
loom_u16 loom_pvs_board_repeat(loom_u16 *held_ticks, loom_u8 *repeat_left,
                               loom_u16 held, loom_u16 delay, loom_u16 period);
loom_u16 loom_pvs_board_move(const LoomBoardPaint *paint, loom_u16 from_shape,
                             loom_s16 from_x, loom_s16 from_y, loom_u16 to_shape,
                             loom_s16 to_x, loom_s16 to_y, loom_u16 kind);
#endif
LoomStatus loom_board_initialize(void);
/* Null clears the resident boards; every cell starts empty. */
LoomStatus loom_board_activate_scene(const LoomBoardScene *scene);

loom_u8 loom_board_width(loom_u8 board);
loom_u8 loom_board_height(loom_u8 board);
/* 0 for a cell outside the board. */
loom_u8 loom_board_get(loom_u8 board, loom_u8 x, loom_u8 y);
LoomStatus loom_board_set(loom_u8 board, loom_u8 x, loom_u8 y, loom_u8 kind);
LoomStatus loom_board_clear(loom_u8 board);

/* A shape at cell x, y (its top-left; negative rows are above the board
 * and count as empty, so a piece may spawn partly above the top). */
loom_u8 loom_board_fits(loom_u8 board, loom_u16 shape, loom_s8 x, loom_s8 y);
LoomStatus loom_board_stamp(loom_u8 board, loom_u16 shape, loom_s8 x, loom_s8 y,
                            loom_u8 kind);
LoomStatus loom_board_erase(loom_u8 board, loom_u16 shape, loom_s8 x, loom_s8 y);
/* A piece's move in one call: `from_shape` at (from_x, from_y) is lifted
 * off, and `to_shape` is stamped at (to_x, to_y) as `kind` if it fits there,
 * or `from_shape` goes back where it was. True when it moved. A turn is a
 * move to another shape; a fall, a slide or a kick is a move to another
 * place. The same as erase, fits and stamp, in one pass on the console. */
/* A hard drop: `shape` at (x, y) is lifted off and stamped as `kind` at the
 * lowest row it fits straight below. Returns the rows it fell, or 0 when
 * the board or kind is invalid. */
loom_u8 loom_board_drop(loom_u8 board, loom_u16 shape, loom_s8 x, loom_s8 y,
                        loom_u8 kind);
loom_u8 loom_board_move(loom_u8 board, loom_u16 from_shape, loom_s8 from_x,
                        loom_s8 from_y, loom_u16 to_shape, loom_s8 to_x,
                        loom_s8 to_y, loom_u8 kind);

/* Falling-piece rules: full rows vanish and the rows above drop; returns
 * how many. Match rules: horizontal and vertical runs of one kind at least
 * min_run long vanish (returns cells cleared), then loom_board_settle drops
 * every cell onto the one below (returns cells moved). */
loom_u8 loom_board_clear_full_rows(loom_u8 board);
loom_u16 loom_board_clear_runs(loom_u8 board, loom_u8 min_run);
loom_u16 loom_board_settle(loom_u8 board);

/* A 16-bit hash of every cell, for the witness and ROM tests. */
loom_u16 loom_board_hash(loom_u8 board);

/* Once a tick, before the hooks: samples every board's pad. loom_board_moves
 * is the LOOM_BUTTON_LEFT / RIGHT / DOWN / UP bits that fired this tick
 * under delayed auto-shift; loom_board_pressed is every button of the same
 * pad that went down this tick, which is how a piece turns. */
LoomStatus loom_board_repeat_update(const LoomInputSnapshot *input);
loom_u16 loom_board_moves(loom_u8 board);
loom_u16 loom_board_pressed(loom_u8 board);
loom_u16 loom_board_held(loom_u8 board);

#endif
