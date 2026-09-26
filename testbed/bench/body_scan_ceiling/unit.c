/* The C that body.asm's loom_pvs_body_scan_ceiling replaced (Loom
 * c027cfd): the ceiling scan over the head from
 * loom_movement_platformer_tick (c027cfd^:runtime/src/movement.c:873-887;
 * actor.c:735-749 is the same loop for the actor bodies, with `top` for
 * `head`) cut out as a function with the assembly routine's name and
 * interface, verbatim. `cell` was a function-scope local of the tick; it
 * is declared here. */
#include "loom_body.h"

loom_s16 loom_pvs_body_scan_ceiling(loom_s16 left, loom_s16 right, loom_s16 head, loom_s16 reach)
{
    loom_u8 cell;
    loom_s16 row_top;
    loom_s16 stopped;

    stopped = -1;
    for (row_top = (loom_s16)((head - 1) & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
         row_top + (LOOM_MOVEMENT_TILE_PIXELS - 1) >= reach && stopped < 0;
         row_top = (loom_s16)(row_top - LOOM_MOVEMENT_TILE_PIXELS)) {
        loom_s16 x;

        for (x = left; x <= right;
             x = (loom_s16)((x | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
            LOOM_MOVEMENT_CELL_AT(cell, x, row_top);
            if (cell == LOOM_MOVEMENT_COLLISION_SOLID) {
                stopped = (loom_s16)(row_top + LOOM_MOVEMENT_TILE_PIXELS);
                break;
            }
        }
    }
    return stopped;
}
