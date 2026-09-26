.include "hdr.asm"
; Link stubs for frame-shell.c (scripts/t6-stubs.py): symbols the
; included runtime unit references but the driver never reaches.
.BASE $00
.RAMSECTION "t6_stubs_frame_shell" BANK $7E SLOT 2
loom_mode1_pose_dirty dsb 4
loom_mode1_raster_tables dsb 4
loom_pvs_generated_asset_chunk_count dsb 4
loom_pvs_generated_asset_chunks dsb 4
loom_pvs_generated_raster_program_count dsb 4
loom_pvs_generated_raster_programs dsb 4
loom_pvs_generated_raster_table_start dsb 4
loom_ui_block dsb 4
.ENDS
