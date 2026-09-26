/* The C rendition of the board hash that board.asm's loom_pvs_board_hash
 * implements (Loom e239e1c:runtime/src/board.c, the host `#else` branch of
 * loom_board_hash: each cell weighted by its odd position, 2 * index + 1),
 * cut out as a function with the assembly routine's name and interface: the
 * board's first cell (&loom_board_cells[spec->cell_offset]) and its count.
 *
 * At e239e1c^ loom_board_hash was a djb2 over the cells (hash * 33 ^ cell,
 * runtime/src/board.c:523-541); the commit changed the hash to this
 * weighted sum so the console could keep it as it paints, and moved it to
 * assembly in the same change. The loop here is the replacement's own C. */
typedef unsigned char loom_u8;
typedef unsigned short loom_u16;

loom_u16 loom_pvs_board_hash(const loom_u8 *cells, loom_u16 count)
{
    loom_u16 hash;
    loom_u16 cell;

    hash = 0u;
    for (cell = 0u; cell < count; ++cell) {
        hash = (loom_u16)(hash + (loom_u16)(cells[cell] * (loom_u16)(2u * cell + 1u)));
    }
    return hash;
}
