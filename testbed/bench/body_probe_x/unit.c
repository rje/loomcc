/* The C that body.asm's loom_pvs_body_probe_x replaced (Loom c027cfd):
 * the wall probe along X from loom_movement_platformer_tick
 * (c027cfd^:runtime/src/movement.c:722-744; actor.c:618-639 is the same
 * loop for the actor bodies) cut out as a function with the assembly
 * routine's name and interface, and the loom_movement_column_solid it calls
 * (movement.c:446-458), verbatim. The loop bound `right + delta` /
 * `left + delta` is `edge + delta`, since edge is right moving right and
 * left moving left; the break that set the wall becomes `return probe`,
 * and no hit returns 0x7fff, as the asm does. */
#include "loom_body.h"

/* True when any cell of the rows `top..bottom` in column `x` is solid. */
loom_u8 loom_movement_column_solid(loom_s16 x, loom_s16 top, loom_s16 bottom)
{
    loom_s16 y;
    loom_u8 cell;

    for (y = top; y <= bottom; y = (loom_s16)((y | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
        LOOM_MOVEMENT_CELL_AT(cell, x, y);
        if (cell == LOOM_MOVEMENT_COLLISION_SOLID) {
            return LOOM_TRUE;
        }
    }
    return LOOM_FALSE;
}

loom_s16 loom_pvs_body_probe_x(loom_s16 edge, loom_s16 delta, loom_s16 top, loom_s16 wall_bottom)
{
    loom_s16 probe;

    for (probe = (loom_s16)(edge + (delta > 0 ? 1 : -1));
         delta > 0 ? probe <= (loom_s16)(edge + delta)
                   : probe >= (loom_s16)(edge + delta);
         probe = (loom_s16)(probe + (delta > 0 ? 1 : -1))) {
        if (loom_movement_column_solid(probe, top, wall_bottom) != LOOM_FALSE) {
            return probe;
        }
        /* Cells are 16 wide: after the first probe, step by tiles. */
        if (delta > 0 ? ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) !=
                         LOOM_MOVEMENT_TILE_PIXELS - 1)
                      : ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) != 0)) {
            probe = delta > 0
                        ? (loom_s16)(probe | (LOOM_MOVEMENT_TILE_PIXELS - 1))
                        : (loom_s16)(probe & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
        }
    }
    return (loom_s16)0x7fff;
}
