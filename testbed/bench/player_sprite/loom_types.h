/* The Loom state player_sprite needs, from Loom 5184be9^
 * (runtime/src/mode1.c's LoomMode1State, runtime/include/loom/mode1.h).
 * Trimmed to the fields the unit and the binding touch: the C reads them by
 * name and the assembly through loom_pvs_mode1_bind's pointers, so neither
 * depends on the rest of the layout. */
#ifndef PLAYER_SPRITE_LOOM_TYPES_H
#define PLAYER_SPRITE_LOOM_TYPES_H

typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)
#define LOOM_OAM_SLOT_MAX ((loom_u8)127u)
#define LOOM_FRAME_OAM_CAPACITY ((loom_u8)33u)

typedef struct LoomMode1Scene {
    loom_u8 sprite_count;
} LoomMode1Scene;

typedef struct LoomMode1State {
    loom_s16 camera_x;
    loom_s16 camera_y;
    loom_u8 initialized;
    const LoomMode1Scene *scene;
    loom_s16 sprite_world_x[LOOM_FRAME_OAM_CAPACITY];
    loom_s16 sprite_world_y[LOOM_FRAME_OAM_CAPACITY];
    loom_u8 sprite_part_count[LOOM_FRAME_OAM_CAPACITY];
    /* Sprite index per OAM slot, 0xff when the slot is unused. */
    loom_u8 slot_index[LOOM_OAM_SLOT_MAX + 1u];
} LoomMode1State;

/* Owned by the driver (mode1.c's static in Loom). */
extern LoomMode1State loom_mode1_state;

void loom_pvs_mode1_bind(loom_s16 *world_x, loom_s16 *world_y,
                         loom_u8 *part_count, loom_u16 sprite_count,
                         const loom_u8 *slot_index, loom_s16 *camera_xy,
                         loom_s16 camera_min_x, loom_s16 camera_min_y,
                         loom_s16 camera_max_x, loom_s16 camera_max_y);
void loom_pvs_player_sprite(loom_u8 slot, loom_s16 world_x, loom_s16 world_y);

#endif
