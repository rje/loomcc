/* loomcc testbed: the Loom types the board.asm pairs share, copied from
 * Loom e239e1c (runtime/include/loom/types.h, board.h, surface.h, video.h).
 * LoomBoardPaint is what board.asm reads by offset: 36 bytes on 816-tcc
 * (four-byte pointers), asserted below where pointers are four bytes. */
#ifndef LOOMCC_BENCH_LOOM_BOARD_H
#define LOOMCC_BENCH_LOOM_BOARD_H

typedef signed char loom_s8;
typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;
typedef loom_u8 LoomStatus;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)
#define LOOM_STATUS_OK ((LoomStatus)0u)
#define LOOM_STATUS_CAPACITY ((LoomStatus)2u)
#define LOOM_STATUS_INVALID_HANDLE ((LoomStatus)4u)
#define LOOM_STATUS_OUT_OF_RANGE ((LoomStatus)5u)

#define LOOM_BOARD_SHAPE_SIZE ((loom_u8)4u)

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
typedef char loom_board_paint_matches_board_asm
    [(sizeof(void *) != 4u || sizeof(LoomBoardPaint) == LOOM_BOARD_PAINT_BYTES) ? 1 : -1];

#endif
