/* The C that body.asm's loom_pvs_body_scan_floor replaced (Loom c027cfd):
 * the floor scan under the feet from loom_movement_platformer_tick
 * (c027cfd^:runtime/src/movement.c:786-813; actor.c:660-686 is the same
 * loop for the actor bodies, with `bottom` for `feet`) cut out as a
 * function with the assembly routine's name and interface, and the
 * loom_movement_floor_in_cell it calls (movement.c:407-421), verbatim.
 * `cell` was a function-scope local of the tick; it is declared here. */
#include "loom_body.h"

/* The feet row a body would stand on within `cell` at column pixel `x`,
 * for a floor cell (solid, one-way or slope) whose row starts at `top`. */
loom_s16 loom_movement_floor_in_cell(loom_u8 cell, loom_s16 x, loom_s16 top)
{
    loom_s16 within;

    within = (loom_s16)(x & (LOOM_MOVEMENT_TILE_PIXELS - 1));
    if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT) {
        return (loom_s16)(top + (LOOM_MOVEMENT_TILE_PIXELS - 1) - within);
    }
    if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
        return (loom_s16)(top + within);
    }
    return (loom_s16)(top - 1);
}

loom_s16 loom_pvs_body_scan_floor(loom_s16 left, loom_s16 right, loom_s16 feet,
                                  loom_s16 reach, loom_s16 sensor_x)
{
    loom_u8 cell;
    loom_s16 row_top;
    loom_s16 landed;

    landed = -1;
    for (row_top = (loom_s16)((feet + 1) & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
         row_top <= reach && landed < 0;
         row_top = (loom_s16)(row_top + LOOM_MOVEMENT_TILE_PIXELS)) {
        loom_s16 x;

        /* The sensor first: it alone reads slopes. */
        LOOM_MOVEMENT_CELL_AT(cell, sensor_x, row_top);
        if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ||
            cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
            loom_s16 floor;

            floor = loom_movement_floor_in_cell(cell, sensor_x, row_top);
            if (floor >= feet - LOOM_MOVEMENT_TILE_PIXELS && floor <= reach) {
                landed = floor;
            }
            continue;
        }
        for (x = left; x <= right;
             x = (loom_s16)((x | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
            LOOM_MOVEMENT_CELL_AT(cell, x, row_top);
            if (cell == LOOM_MOVEMENT_COLLISION_SOLID ||
                (cell == LOOM_MOVEMENT_COLLISION_ONE_WAY && feet < row_top)) {
                landed = (loom_s16)(row_top - 1);
                break;
            }
        }
    }
    return landed;
}
