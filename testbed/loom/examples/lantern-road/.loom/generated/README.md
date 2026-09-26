# Loom generated files

Project: Lantern Road (`0000000000006a0518cf599e2bfff0b0`)

This entire directory is deterministic, disposable output owned by Loom. Edit portable
game code under `Code/Portable` and backend-specific C or assembly under
`Code/Target/pvsneslib`; Loom never writes to either authored root.

The generated catalog currently contains 8 assets, 4 scenes, 29 entities, 1 worlds,
and 1 enabled user hooks. `ir/project-ir.json` retains complete source locations; the C
files expose readable target-neutral static records. `plan/project-plan.json` assigns 66
opaque asset segments to 70 mapper sections across 16 LoROM banks and selects 13 runtime
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
