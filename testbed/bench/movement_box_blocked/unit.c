/* The C that runtime/backends/pvsneslib/src/movement.asm replaced
 * (Loom f0a6cd0^:runtime/src/movement.c:98-117, the body of
 * loom_movement_box_blocked from the bounds check through the cell scan),
 * cut out as a function with the assembly routine's name and interface:
 * the scene's fields arrive by value. */

typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

#define LOOM_MOVEMENT_TILE_PIXELS ((loom_u16)16u)
#define LOOM_MOVEMENT_COLLISION_NONE ((loom_u8)0u)
#define LOOM_MOVEMENT_COLLISION_SOLID ((loom_u8)1u)

loom_u8 loom_pvs_movement_box_blocked(const loom_u8 *cells,
                                      loom_u16 collision_width,
                                      loom_u16 pixel_width,
                                      loom_u16 pixel_height,
                                      loom_s16 left,
                                      loom_s16 top,
                                      loom_s16 right,
                                      loom_s16 bottom)
{
    loom_u16 first_x;
    loom_u16 last_x;
    loom_u16 first_y;
    loom_u16 last_y;
    loom_u16 x;
    loom_u16 y;

    if (left < 0 || top < 0 || right >= (loom_s16)pixel_width ||
        bottom >= (loom_s16)pixel_height) {
        return LOOM_MOVEMENT_COLLISION_SOLID;
    }

    first_x = (loom_u16)left / LOOM_MOVEMENT_TILE_PIXELS;
    last_x = (loom_u16)right / LOOM_MOVEMENT_TILE_PIXELS;
    first_y = (loom_u16)top / LOOM_MOVEMENT_TILE_PIXELS;
    last_y = (loom_u16)bottom / LOOM_MOVEMENT_TILE_PIXELS;
    for (y = first_y; y <= last_y; ++y) {
        const loom_u8 *row;

        /* One multiply per row: 816-tcc calls a helper for each one. */
        row = cells + (loom_u16)(y * collision_width);
        for (x = first_x; x <= last_x; ++x) {
            if (row[x] != LOOM_MOVEMENT_COLLISION_NONE) {
                return LOOM_MOVEMENT_COLLISION_SOLID;
            }
        }
    }
    return LOOM_MOVEMENT_COLLISION_NONE;
}
