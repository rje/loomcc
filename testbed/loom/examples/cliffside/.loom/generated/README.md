# Loom generated files

Project: Cliffside (`0c11f51de00000000000000000000001`)

This entire directory is deterministic, disposable output owned by Loom. Edit portable
game code under `Code/Portable` and backend-specific C or assembly under
`Code/Target/pvsneslib`; Loom never writes to either authored root.

The generated catalog currently contains 9 assets, 1 scenes, 25 entities, 1 worlds,
and 2 enabled user hooks. `ir/project-ir.json` retains complete source locations; the C
files expose readable target-neutral static records. `plan/project-plan.json` assigns 25
opaque asset segments to 28 mapper sections across 8 LoROM banks and selects 13 runtime
modules from actual authored use. `plan/graphics-plan.json` binds exact importer costs to
the generated CGRAM, tilemap, fixed VRAM residency, OAM poses, and conservative scanline
pressure used by each Mode 1 scene. `plan/frame-plan.json` records every forced-blank
scene-load DMA job and continuation commit plus target-private HDMA channels, registers,
scanline ranges, table memory, and conflict checks. `plan/memory-plan.json` records exact
planned ROM sections and converter-measured cue/sample/echo SPC costs; the build binds
linker-measured WRAM and ROM occupancy to that input.
Target-private assembly/table files implement those
assignments without exposing banked pointers through portable headers, and
`toolchain/pvsneslib-inputs.json` is the deterministic host-build handoff.
