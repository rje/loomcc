/* player_sprite: the player tick's last step, the player's sprite moving in
 * Mode 1's world tables, over a scene of 12 sprites: the hero is a
 * three-part metasprite (slot 0, index 0), a two-part enemy (slot 5,
 * index 4), single sprites, a metasprite at the end of the list whose parts
 * run past sprite_count, and slots with no sprite. Twelve placements with
 * constant arguments, as a walking tick makes them. */
#include "bench.h"
#include "loom_types.h"

#define SPRITES 12

LoomMode1State loom_mode1_state;
static const LoomMode1Scene scene = {SPRITES};

void bench_setup(void)
{
    unsigned short i;

    loom_mode1_state.initialized = LOOM_TRUE;
    loom_mode1_state.scene = &scene;
    for (i = 0; i <= LOOM_OAM_SLOT_MAX; i++)
        loom_mode1_state.slot_index[i] = 0xffu;
    for (i = 0; i < LOOM_FRAME_OAM_CAPACITY; i++) {
        loom_mode1_state.sprite_world_x[i] = (loom_s16)(0x100 + i);
        loom_mode1_state.sprite_world_y[i] = (loom_s16)(0x200 + i);
        loom_mode1_state.sprite_part_count[i] = 0u;
    }
    /* slot -> index: 0 -> 0 (+2 parts), 5 -> 4 (+1 part), 9 -> 3,
     * 12 -> 6, 20 -> 7, 31 -> 10 (+3 parts, only one fits). */
    loom_mode1_state.slot_index[0] = 0u;
    loom_mode1_state.sprite_part_count[0] = 2u;
    loom_mode1_state.slot_index[5] = 4u;
    loom_mode1_state.sprite_part_count[4] = 1u;
    loom_mode1_state.slot_index[9] = 3u;
    loom_mode1_state.slot_index[12] = 6u;
    loom_mode1_state.slot_index[20] = 7u;
    loom_mode1_state.slot_index[31] = 10u;
    loom_mode1_state.sprite_part_count[10] = 3u;
    loom_pvs_mode1_bind(loom_mode1_state.sprite_world_x,
                        loom_mode1_state.sprite_world_y,
                        loom_mode1_state.sprite_part_count, SPRITES,
                        loom_mode1_state.slot_index, &loom_mode1_state.camera_x,
                        0, 0, 256, 32);
}

void bench_run(void)
{
    loom_pvs_player_sprite(0u, 40, 176);
    loom_pvs_player_sprite(0u, 42, 175);
    loom_pvs_player_sprite(5u, 300, 160);
    loom_pvs_player_sprite(9u, -8, 96);
    loom_pvs_player_sprite(12u, 511, -16);
    loom_pvs_player_sprite(20u, 128, 128);
    loom_pvs_player_sprite(31u, 1000, 200);
    loom_pvs_player_sprite(3u, 77, 77);
    loom_pvs_player_sprite(127u, 66, 66);
    loom_pvs_player_sprite(0u, 45, 173);
    loom_pvs_player_sprite(5u, 296, 160);
    loom_pvs_player_sprite(0u, 49, 170);
}

void bench_check(void)
{
    unsigned short i;

    for (i = 0; i < 14; i++)
        BENCH_OUT(loom_mode1_state.sprite_world_x[i]);
    for (i = 0; i < 14; i++)
        BENCH_OUT(loom_mode1_state.sprite_world_y[i]);
}
