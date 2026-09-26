#include <loom-pvsneslib/generated.h>
#include <loom/generated/raster_programs.h>

/* Adapter-private ownership: HDMA channel 6. ROM tables and the wave's
   data share one array; the runtime's WRAM tables are its own. */
const loom_u8 loom_pvs_generated_raster_table_start[] = {
0x38u, 0xe0u, 0x84u, 0x38u, 0xe0u, 0x88u, 0x38u, 0xe0u, 0x8cu, 0x38u, 0xe0u, 0x90u, 0x00u
};

const loom_u16 loom_pvs_generated_raster_program_count = 1u;

const LoomPvsGeneratedRasterProgram loom_pvs_generated_raster_programs[1] = {
    { LOOM_GENERATED_RASTER_PROGRAM_0, 13u, 6u, 0x40u, 2u, 0x32u, LOOM_PVS_RASTER_KIND_FIXED_COLOR, 0u, 0u, 13u, 0u },
};
