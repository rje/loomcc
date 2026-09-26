/* The pieces of Loom's runtime/include/loom/movement.h (at c027cfd^) the
 * platformer bodies' tile probes use: the types, the tile and cell
 * constants, the exported grid and the cell lookup macro, verbatim. */
#ifndef LOOM_BODY_BENCH_H
#define LOOM_BODY_BENCH_H

typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)

#define LOOM_MOVEMENT_TILE_PIXELS ((loom_u16)16u)
#define LOOM_MOVEMENT_MAX_COLLISION_ROWS ((loom_u16)64u)

#define LOOM_MOVEMENT_COLLISION_NONE ((loom_u8)0u)
#define LOOM_MOVEMENT_COLLISION_SOLID ((loom_u8)1u)
#define LOOM_MOVEMENT_COLLISION_ONE_WAY ((loom_u8)2u)
#define LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ((loom_u8)3u)
#define LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT ((loom_u8)4u)

typedef struct LoomMovementGrid {
    const loom_u8 *cells;
    loom_s16 pixel_width;
    loom_s16 pixel_height;
    loom_u16 row_offsets[LOOM_MOVEMENT_MAX_COLLISION_ROWS];
} LoomMovementGrid;
extern LoomMovementGrid loom_movement_grid;
/* body.asm reads this struct by offset (cells at 0, the bounds at 4 and 6,
 * the row offsets from 8). */
#define LOOM_MOVEMENT_GRID_BYTES (8u + 2u * LOOM_MOVEMENT_MAX_COLLISION_ROWS)

/* `result` = the cell at pixel (px, py), or solid outside the room. */
#define LOOM_MOVEMENT_CELL_AT(result, px, py)                                    \
    do {                                                                         \
        loom_s16 loom_cell_x_ = (px);                                            \
        loom_s16 loom_cell_y_ = (py);                                            \
        if (loom_cell_x_ < 0 || loom_cell_y_ < 0 ||                              \
            loom_cell_x_ >= loom_movement_grid.pixel_width ||                    \
            loom_cell_y_ >= loom_movement_grid.pixel_height) {                   \
            (result) = LOOM_MOVEMENT_COLLISION_SOLID;                            \
        } else {                                                                 \
            (result) = loom_movement_grid.cells                                  \
                [loom_movement_grid.row_offsets[(loom_u16)loom_cell_y_ >> 4] +  \
                 ((loom_u16)loom_cell_x_ >> 4)];                                 \
        }                                                                        \
    } while (0)

loom_s16 loom_pvs_body_probe_x(loom_s16 edge, loom_s16 delta, loom_s16 top, loom_s16 wall_bottom);
loom_s16 loom_pvs_body_scan_floor(loom_s16 left, loom_s16 right, loom_s16 feet, loom_s16 reach, loom_s16 sensor_x);
loom_s16 loom_pvs_body_scan_ceiling(loom_s16 left, loom_s16 right, loom_s16 head, loom_s16 reach);

#endif
