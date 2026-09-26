# loomcc benchmarks

Each directory here is one benchmark: a C unit under test, a driver that feeds
it fixed inputs, and (for the pairs taken from Loom's history) the hand-written
65816 assembly that replaced that C in Loom. One ROM measures one benchmark in
one variant:

| variant | unit compiled by | driver | purpose |
|---|---|---|---|
| `host` | clang (host, 32-bit `int`) | clang | reference result words |
| `tcc` | 816-tcc + 816-opt | 816-tcc | the baseline to beat |
| `asm` | `unit.asm` (Loom's hand assembly) | 816-tcc | the target (pairs only) |
| `loomcc` | loomcc (`$LOOMCC -S`) | 816-tcc | the compiler under development |

Results live in [BASELINE.md](BASELINE.md).

## Running

```
testbed/harness/run.sh testbed/bench/<name> <host|tcc|asm|loomcc> <outdir>
testbed/harness/run_all.py <outdir> [--variants host,tcc,asm,loomcc] [name ...]
```

`run.sh` prints a one-line JSON summary and the result words, and writes
`<outdir>/results.json` plus the raw files: `build/` (every `.asm`, `.obj`,
`bench.sfc`, `bench.sym`, `link.log`), `run/trace.csv` (the watched harness
globals per frame) and `profile/profile.txt` (loom-emulator's per-label
instruction profile of exactly the `bench_run` frames). `run_all.py` runs every
benchmark in every variant it has, checks the words agree across variants and
prints the markdown table used in BASELINE.md.

Environment overrides: `PVSNESLIB_HOME` (toolchain root, default
`~/Library/Loom/Toolchains/v0/artifacts/pvsneslib`), `LOOM_EMULATOR`
(default `$LOOM_REPO/target/debug/loom-emulator`), `LOOMCC`
(default `<repo>/target/debug/loomcc`), `LOOMCC_BENCH_CACHE` (where the
calibration result is cached, default `$TMPDIR/loomcc-bench`). Every tool and
emulator run is started under `nice -n 19`.

## The contract

A benchmark directory holds:

- **`unit.c`** — the C under test. It defines the function(s) under test and any
  globals they own. It is the only file whose compiler changes between the
  `tcc` and `loomcc` variants.
- **`unit.asm`** (pairs only) — hand-written assembly defining the same
  functions with the same names and the 816-tcc ABI, plus any RAM or helpers
  they need; it starts with `.include "hdr.asm"`. The `asm` variant links it
  *instead of* `unit.c`, so anything both variants need that the unit does not
  own (inputs, state the unit reads or writes, callees that are not under
  test) is defined in `driver.c`.
- **`driver.c`** — always compiled by 816-tcc. It includes `bench.h` and
  defines `bench_setup()` (build the inputs), `bench_run()` (only the calls to
  the unit, with constant arguments where possible, saving the return values)
  and `bench_check()` (write result words with `BENCH_OUT(v)` into
  `bench_out[32]`; they cover every returned value and every piece of state
  the unit writes, folded into hashes where the state is large).
- **`bench.toml`** (or `bench.json`) — `name`, `kind` (`pair` / `micro`),
  `description`, `unit_labels` (the functions under test: the build fails if
  the unit does not define them), `helper_labels` (library routines the unit
  calls, e.g. `tcc__mul`, `tcc__udiv`, `memcpy`: their instructions are counted
  separately), optional `include_dirs` (relative to the bench), `defines`,
  `snes_defines`, `host_defines`, `notes`, and for pairs a `[provenance]`
  table: `loom_commit`, `c_source` (file:lines at the commit's parent),
  `asm_source` (file:label at the commit, and whether it changed by HEAD).

Rules every benchmark follows:

- **Valid only when equal.** `host`, `tcc` and `asm` must print the same words.
  The host build has a 32-bit `int`, so the C narrows explicitly (`(u16)`)
  wherever it would rely on 16-bit wraparound.
- **Under a frame.** `bench_run` must finish inside one frame (357368 master
  clocks). The tcc variant of every benchmark stays under about 280000 so there
  is headroom; the harness fails loudly when a run takes about a frame.
- **Lean driver.** `bench_run` does nothing but call the unit, so the
  clocks are the unit's plus the 816-tcc call sequence (argument pushes,
  `jsl`, pops) that every variant pays alike.
- **Only 816-tcc-compatible C** in units: `char`/`short`/`int` (16-bit),
  unsigned variants, pointers, structs, arrays, function pointers; no `long`,
  no floating point.

## The harness (testbed/harness)

`harness.asm` is `main` (crt0 calls it with D = 0, DBR = $7E, native mode,
A/X/Y 16-bit):

1. zero the harness globals, `jsl bench_setup`, `bench_phase = 1`;
2. `sei`, NMI/IRQ/auto-joypad off (`$4200 = 0`), `$4201` bit 7 set so a `$2137`
   read latches the PPU counters, VTIME = 224 and V-IRQ enabled (masked by
   `sei`: it only sets TIMEUP, `$4211` bit 7, once a frame);
3. wait for the start of a VBlank twice (poll `$4212`), `bench_phase = 2`;
4. read `$4211` (clear TIMEUP), latch H/V (`$2137`, then `$213C`/`$213D` twice
   each for the 9-bit values, `$213F` to reset the byte flip-flops) into
   `bench_h0`/`bench_v0`, `jsl bench_run`, latch again into
   `bench_h1`/`bench_v1`, read `$4211` bit 7 into `bench_wrap`;
5. `bench_phase = 3`, wait for two more VBlank starts, `jsl bench_check`,
   `bench_phase = 4`, `bench_done = $600d`, idle in `harness_idle`.

`bench.py` (behind `run.sh`) builds the ROM exactly as Loom's pipeline does
(`816-tcc -F -Wall -c` → `816-opt` → `wla-65816 -d -s -x`, linked with
`wlalink -d -s -A -c` beside PVSnesLib's LoROM FastROM `crt0_snes.obj`,
`libc.obj`, `libm.obj`, `libtcc.obj`, with `hdr.asm` filled from
`hdr.asm.in` as `snes_rules` does: LoROM, FastROM, 8 banks). Before
assembling, every ROM section the unit defines is renamed `UNIT_<name>` (not
`APPENDTO` sections), so the linked `.sym` bounds exactly the unit's code with
`SECTIONSTART_UNIT_*`/`SECTIONEND_UNIT_*`.

It then runs loom-emulator twice:

1. `trace` with watches on the harness globals until `bench_done = $600d`: the
   frames where `bench_phase` goes from 1 to 3 are the `bench_run` frames, and
   the last row gives the latches and `bench_out`;
2. `trace --profile` over exactly those frames (setup and check happen in other
   frames, and NMI is off, so nothing else runs there but the harness's wait
   loops). The symbol file given to the profiler is the linked one plus a label
   `sec:<section>` at every ROM section start, so no code is charged to a label
   in another section (the profiler ignores labels starting `__`, `_far`,
   `_skip` or holding `@`).

`results.json` reports:

| field | meaning |
|---|---|
| `out` | the `bench_out` words |
| `code_bytes`, `rodata_bytes`, `sections` | ROM bytes of the unit's sections (`.rodata` separately) |
| `unit_instructions` | instructions executed at labels inside the unit's sections |
| `helper_instructions` | instructions in the sections holding `helper_labels` |
| `non_harness_instructions` | everything but the harness's own labels (unit + helpers + the driver's `bench_run`) |
| `labels` | the per-label counts behind those sums |
| `latch`, `elapsed_master_clocks` | `(v1 - v0) * 1364 + (h1 - h0) * 4` |
| `harness_overhead_master_clocks`, `net_master_clocks` | the calibrated fixed cost and elapsed minus it |

**Calibration.** `testbed/harness/calib` has an empty `bench_run`; its elapsed
time is the harness overhead (the `jsl bench_run`/`rtl` and the latch reads
between the two latches): **480 master clocks**. `net_master_clocks` subtracts
it, so it is the time of `bench_run`'s body: the unit, its helpers and the
driver's call sequences. The calibration is cached per harness/toolchain hash.
Check: a unit of 1000 `nop`s (12 master clocks each in FastROM) measures
12456 net = 96 (the driver's `jsl`/`rtl` to it) + 12000 + 360 of DRAM refresh
(40 master clocks per scanline).

**Accuracy.** The H counter counts 4-clock dots except two 6-clock dots per
line, so a difference can be off by a few clocks; refresh lands wherever the
run crosses dot 134 of a line. Expect ±~40 master clocks between otherwise
equal builds whose code sits at different alignments. Instruction counts are
exact (the profile covers at most two frames and is rounded from per-frame
averages with one decimal). A run that reaches line 224 again sets TIMEUP and
the harness refuses it ("took about a frame or longer").

## Plugging in loomcc

`testbed/harness/bench.py`, `build_rom()`, the `variant == "loomcc"` branch
(marked `>>> loomcc plugs in here`) runs

```
$LOOMCC -S -I<harness> -I<bench> [-I<include_dirs>...] [-D<defines>...] unit.c -o unit.raw.asm
```

in the build directory. The output must be a WLA-DX 65816 source that
assembles standalone with `wla-65816 -d -s -x` in a directory holding
`hdr.asm` (start it with `.include "hdr.asm"`), defines every function in
`unit_labels` with the 816-tcc ABI entry under its C name, and puts its RAM
in `.RAMSECTION ... BANK $7E SLOT 2` (after `.BASE $00`). Everything after that
is shared with the other variants: the sections are renamed `UNIT_*`, the
driver (always 816-tcc) calls the entries, and the counts come out the same
way. Consequences:

- every piece of code loomcc emits for the unit (internal bodies, ABI entries,
  its own runtime helpers) must be in that `.asm` to count as the unit's; a
  helper linked from elsewhere must be listed in `helper_labels` or it shows up
  only in `non_harness_instructions`;
- the driver calls through the 816-tcc ABI, so the ABI entry's cost is part of
  what is measured, as it is in Loom;
- `run_all.py <out> --variants host,tcc,asm,loomcc` checks loomcc's words
  against the others.

## Benchmarks

### Pairs (C/asm from Loom's history)

Every pair prints identical words in host, tcc and asm. Caveats (inputs chosen to avoid a known C/asm divergence, glue that is not Loom code, stubs) are in each bench.toml's `notes`.

| bench | Loom commit | what | C (the replaced code) | asm |
|---|---|---|---|---|
| `actor_body_step` | 13d8987 | The actor platformer body step on its record: ledge turn, velocities and clamps, jump and cut, gravity, the X move against walls and the Y tile scan. Six bodies in a 20x14 room. | runtime/src/actor.c:509-673, 744-752 and 776-779 at 13d8987^ (loom_actor_platformer_step through the Y scans, console path with LOOM_BODY_PROBES_FAST), moved onto the LoomActorBodyStep record 13d8987 introduced | runtime/backends/pvsneslib/src/body.asm:loom_pvs_actor_body (with loom_pvs_actor_pointers/_s8/_advance and loom_pvs_body_cell/_grid) at 13d8987 (at HEAD it survives as loom_pvs_actor_core behind loom_pvs_actor_body, with the probes inlined and the RAM moved) |
| `animation_pass` | 5184be9 | Animation tick: count each playing clip's elapsed ticks and list the players whose frame ran its duration. 9 players over 5 ticks. | runtime/src/animation.c:387-420 at 5184be9^ (loom_animation_update's per-player loop up to the due test) | runtime/backends/pvsneslib/src/body.asm:loom_pvs_animation_pass (+ loom_pvs_animation_bind, the LOOM_ACTOR_* macros and the RAM they use) at 5184be9; unchanged at HEAD (HEAD's copy used) |
| `board_drop` | cb5a47f | Hard drop: lift a piece off the board, find the lowest row it fits straight below, stamp it there (cells, tilemap words, dirty spans, kept hash). One two-cell drop on a 10x20 board. | loom_board_drop is new in cb5a47f; the C is its host #else branch there (runtime/src/board.c: erase, fits one row lower until it fails, stamp) over the parent's C calls: runtime/src/board.c at cb5a47f^ / e239e1c^ loom_board_fits (264-296), loom_board_paint_shape (298-333), loom_board_set, loom_board_draw_cell, loom_board_cell, and surface.c's loom_surface_dirty/_cell/_row/_mark, taking the LoomBoardPaint record | runtime/backends/pvsneslib/src/board.asm:loom_pvs_board_drop (+ _move_from, _fits_core, _paint_core, _draw, _rehash, _dirty, _load, _mul) at cb5a47f (unchanged at HEAD) |
| `board_fill` | cb5a47f | Board wipe (loom_board_clear): every cell becomes 0, its tilemap words are rewritten and the surface's dirty spans widened; the kept hash goes stale. A full 4x2 box and a full 2x1 board of 2x2-tile cells. | runtime/src/board.c:292-308 at cb5a47f^ (loom_board_clear: loom_board_touch 38-46, the cell loop, loom_board_redraw_rows 169-212 over every row) with loom_board_cell 163-167 and runtime/src/surface.c:169-202,318-343 at cb5a47f^ (loom_surface_dirty, _cell, _row, _mark), taking the LoomBoardPaint record and the kind as an argument | runtime/backends/pvsneslib/src/board.asm:loom_pvs_board_fill (+ loom_pvs_board_draw, _dirty, _load, _mul) at cb5a47f (unchanged at HEAD) |
| `board_fit` | e239e1c | Falling-piece fit test: is every cell of a 4x4 shape on the board and empty? 7 tetromino probes of a 10x20 board. | runtime/src/board.c:264-296 at e239e1c^ (loom_board_fits, with loom_board_cell 118-122), taking the LoomBoardPaint record instead of a board index | runtime/backends/pvsneslib/src/board.asm:loom_pvs_board_fits (+ loom_pvs_board_fits_core, loom_pvs_board_load, loom_pvs_board_mul) at e239e1c (unchanged at HEAD) |
| `board_paint` | e239e1c | Falling-piece stamp/erase: write a 4x4 shape's cells, their tilemap words, the surface's dirty spans and the kept board hash. Two paints on a 10x20 board (a T stamp, an I clipped above the top). | runtime/src/board.c:298-333 at e239e1c^ (loom_board_paint_shape) with loom_board_set 229-245, loom_board_draw_cell 170-200, loom_board_cell 118-122, and runtime/src/surface.c:159-192,287-312 at e239e1c^ (loom_surface_dirty, loom_surface_cell, loom_surface_row, loom_surface_mark), taking the LoomBoardPaint record | runtime/backends/pvsneslib/src/board.asm:loom_pvs_board_paint (+ _paint_core, _draw, _rehash, _dirty, _load, _mul) at e239e1c (unchanged at HEAD) |
| `body_probe_x` | c027cfd | Platformer body wall probe along X: the first solid column between the edge and edge + delta over a row range. 12 probes over a 24x14 room. | runtime/src/movement.c:722-744 at c027cfd^ (the X-probe loop in loom_movement_platformer_tick; actor.c:618-639 is the same loop for actor bodies) plus loom_movement_column_solid (movement.c:446-458) and LOOM_MOVEMENT_CELL_AT (include/loom/movement.h:205-218) | runtime/backends/pvsneslib/src/body.asm:loom_pvs_body_probe_x (+ loom_pvs_body_probe_x_column, loom_pvs_body_cell, loom_pvs_body_grid, LOOM_SIGNED_CMP) at c027cfd (unchanged at HEAD) |
| `body_scan_ceiling` | c027cfd | Platformer body ceiling scan: the row under the first solid cell between the head and the reach above it. 11 scans over a 24x14 room. | runtime/src/movement.c:873-887 at c027cfd^ (the ceiling-scan loop in loom_movement_platformer_tick; actor.c:735-749 is the same loop for actor bodies) plus LOOM_MOVEMENT_CELL_AT (include/loom/movement.h:205-218) | runtime/backends/pvsneslib/src/body.asm:loom_pvs_body_scan_ceiling (+ loom_pvs_body_cell, loom_pvs_body_grid, LOOM_SIGNED_CMP) at c027cfd (unchanged at HEAD) |
| `body_scan_floor` | c027cfd | Platformer body floor scan: the first floor (solid, one-way from above, or a slope under the sensor) between the feet and the reach. 14 scans over a 24x14 room. | runtime/src/movement.c:786-813 at c027cfd^ (the floor-scan loop in loom_movement_platformer_tick; actor.c:660-686 is the same loop for actor bodies) plus loom_movement_floor_in_cell (movement.c:407-421) and LOOM_MOVEMENT_CELL_AT (include/loom/movement.h:205-218) | runtime/backends/pvsneslib/src/body.asm:loom_pvs_body_scan_floor (+ loom_pvs_body_cell, loom_pvs_body_grid, LOOM_SIGNED_CMP) at c027cfd (unchanged at HEAD) |
| `camera_follow` | 925fc3a | The camera's tick: target lookup, regions, centre/dead-zone/screens follow with look-ahead, auto-scroll, clamp, smoothing and Mode 1's store. Eight ticks through seven scenes. | runtime/src/camera.c:43-278 at 925fc3a^ (loom_camera_active_settings, _auto_step, _axis_goal, _approach, _apply) plus the Mode 1 calls it makes, runtime/src/mode1.c:65-76, 1386-1416, 1454-1474, 1662-1670 at 925fc3a^ (loom_mode1_clamp, _set_camera, _sprite_index, _sprite_position, _camera_x/_y) | runtime/backends/pvsneslib/src/body.asm:loom_pvs_camera_update (with loom_pvs_camera_half/_axis_goal/_approach/_auto and loom_pvs_player_umul/_udiv) at 925fc3a (changed by HEAD: HEAD tests the facing's sign bit instead of comparing it with 1, and adds unrelated routines) |
| `contact_scan` | 925fc3a | Combat's contact query: the first live, awake actor whose hit box covers the player's, from a start slot. Eight actors, four player boxes, every hit walked. | runtime/src/combat.c:120-140 (loom_combat_actor_touches_player) and :447-469 (the head of loom_combat_update's contact loop through the shot check) at 925fc3a^ | runtime/backends/pvsneslib/src/body.asm:loom_pvs_combat_touch at 925fc3a (changed by HEAD: HEAD computes the player's box itself from loom_movement_state and keeps the array pointers in the direct page) |
| `display_block` | 72f1cd4 | Mode 1 frame build's display block: backdrop, brightness, main layers, sprite size pair, raster binding and forced blank, reporting whether colour math and the raster drive are due. 12 calls over three scenes. | runtime/src/mode1.c:1377-1402 at 72f1cd4^ (loom_mode1_build_frame's display block after the scroll) | runtime/backends/pvsneslib/src/oam.asm:loom_pvs_mode1_display at 72f1cd4 (unchanged at HEAD) |
| `layer_scroll` | cbd8c0c | Mode 1 frame build's layer scroll: BG1 on the camera, parallax layers at camera*num/den, a cache for a still camera. 8 calls over three scenes. | runtime/src/mode1.c:1264-1331 at cbd8c0c^ (loom_mode1_build_frame's scroll block) plus loom_mode1_scaled and loom_mode1_denominator_shift (mode1.c:919-951) | runtime/backends/pvsneslib/src/oam.asm:loom_pvs_mode1_scroll (+ loom_pvs_scroll_scaled) at cbd8c0c; code unchanged at HEAD, whose RAM section (used here) moved from BANK 0 SLOT 1 to BANK $7e SLOT 2 |
| `movement_box_blocked` | f0a6cd0 | Tile-collision probe: is any 16-pixel cell under a box solid? 24 boxes over a 20x14 room. | runtime/src/movement.c:98-117 at f0a6cd0^ (inside loom_movement_box_blocked) | runtime/backends/pvsneslib/src/movement.asm:loom_pvs_movement_box_blocked at f0a6cd0 (unchanged at HEAD) |
| `oam_batch` | 7b4d960 | Mode 1 sprite build: screen position, view cull and OAM staging for a 10-sprite scene, then a two-sprite batch with a rejected duplicate slot. | runtime/src/mode1.c:526-580 at 7b4d960^ (loom_mode1_add_sprites; the loop body verbatim, reading the LoomMode1SpriteBatch descriptor as the host rendition at 7b4d960 mode1.c:548-598 does) + runtime/src/frame-build.c:143-154 (loom_frame_build_reserve_oam) and :202-209 (the submit loop staging each reserved entry) at 7b4d960^; the per-entry stage, which was already oam.asm:loom_pvs_oam_stage on the console at 7b4d960^, is given as its host C rendition (frame-transaction.c:134-158 + runtime-adapter.c:974-1012 at 2e278a7) so the C side is all C | runtime/backends/pvsneslib/src/oam.asm:loom_pvs_oam_batch (+ its RAM section and high-table tables) at 7b4d960; changed later (HEAD strides 20-byte sprite records and adds a fast path for unchanged poses); `.BASE $00` / `.BASE $80` added around the RAM section as HEAD has it |
| `oam_finish` | 2e278a7 | End of an OAM commit: park the 11 of last commit's 32 slots this commit did not stage, then make this commit's 29 slots the previous set. | runtime/backends/pvsneslib/src/runtime-adapter.c:953-973 at 2e278a7 (the hide loop of loom_pvs_target_prepare_oam, the host C rendition of the same contract) plus its previous-slot bookkeeping; the 2e278a7^ C (a full oamClear every commit) was replaced rather than transliterated and is not isolatable | runtime/backends/pvsneslib/src/oam.asm:loom_pvs_oam_finish (+ its RAM section and high-table tables) at 2e278a7; changed later (7b4d960 advances the generation at finish, HEAD also clears per-slot staged flags); `.BASE $00` / `.BASE $80` added around the RAM section as HEAD has it |
| `oam_stage` | 2e278a7 | Stage one sprite into the OAM shadow: validate, reject duplicate slots by generation mark, write the low and high tables. Twenty entries, nine rejected. | runtime/backends/pvsneslib/src/frame-transaction.c:134-158 (validation + slot mark, loom_port_oam_stage's host branch) and runtime-adapter.c:974-1012 (the shadow write, loom_pvs_target_prepare_oam's last loop body) at 2e278a7 -- the host C rendition of the same contract; the 2e278a7^ C (array staging, insertion sort, oamClear + pvsneslib oamSet/oamSetEx at commit) was rewritten rather than transliterated and is not isolatable | runtime/backends/pvsneslib/src/oam.asm:loom_pvs_oam_stage (+ its RAM section and high-table tables) at 2e278a7 (the routine is unchanged at HEAD); `.BASE $00` / `.BASE $80` added around the RAM section as HEAD has it |
| `player2_position` | a937adb | The second player's position query: the controller slot's x and y through two out-pointers while it is alive and joined. 8 queries over a 32-slot actor pool. | runtime/src/actor.c:239-253 at a937adb^ (loom_actor_controller_position), with LoomActorPool from include/loom/actor.h:306-366 | runtime/backends/pvsneslib/src/body.asm:loom_actor_controller_position at a937adb (unchanged at HEAD) |
| `player_sprite` | 5184be9 | The player's sprite (and its metasprite parts) moving in Mode 1's world tables through the slot index. 12 placements over a 12-sprite scene. | runtime/src/mode1.c:1416-1464 at 5184be9^ (loom_mode1_sprite_index + loom_mode1_set_sprite_position, which movement.c called after the assembly player tick) | runtime/backends/pvsneslib/src/body.asm:loom_pvs_place_sprite and the sprite lookup at the end of loom_pvs_player_tail at 5184be9 (unchanged at HEAD, HEAD's copy used), plus loom_pvs_mode1_bind for setup |
| `player_tick` | 925fc3a | The player's platformer tick: run and dash caps, skid, jump buffer, the run-scaled jump and its cut, gravity, the X move with the slope step, the floor and ceiling scans and the slope under the feet. Three ticks in a 20x14 room. | runtime/src/movement.c:412-465 (loom_movement_floor_in_cell, _slope_feet, _column_solid) and 480-936 (loom_movement_platformer_tick) at 925fc3a^ | runtime/backends/pvsneslib/src/body.asm:loom_pvs_player_tick with loom_pvs_tight_cell/_advance/_probe_x/_scan_floor/_scan_ceiling and loom_pvs_player_ease/_slope_feet/_jump_scale/_umul/_udiv at 925fc3a |
| `scene_trigger_scan` | 7b4d960 | Trigger scan box test: bit mask of the triggers under the player box. Ten triggers, nine probes. | runtime/src/scene.c:234-246 at 7b4d960^ (the box test inside loom_scene_check_triggers' trigger loop, gathered into a mask; 7b4d960 keeps the same test as the host rendition loom_scene_intersect_mask, scene.c:145-164) | runtime/backends/pvsneslib/src/scene.asm:loom_pvs_scene_intersect_mask at 7b4d960 (at HEAD only the 24-byte stride became a .DEFINE; code otherwise unchanged) |
| `solid_actor_query` | ea4e3af | The pool's solid-actor queries: the highest visible solid actor top under a box (a floor) and whether a solid actor covers a box (a wall). Eight actors, five solid; five queries of each. | runtime/src/movement.c:48-60 (the shims), crates/loom-app/src/generate/output.rs:6128-6131 (the generated hooks), runtime/src/actor.c:1480-1534 and 1560-1604 (loom_actor_floor_below, loom_actor_blocks_box), runtime/src/mode1.c:1663-1666 (loom_mode1_visibility_table) at ea4e3af^ | runtime/backends/pvsneslib/src/body.asm:loom_pvs_solid_next, loom_pvs_solid_floor, loom_pvs_solid_wall at ea4e3af (unchanged at HEAD), behind two small 816-tcc ABI entries written for the bench (they store the box in loom_pvs_sq_* and jsr, as body.asm's loom_pvs_probe_actor_floor/_wall do) |
| `surface_flush` | e239e1c | Surface flush: walk the dirty tilemap rows from the cursor and write one VRAM DMA job per run into the frame build within the tick's job and byte budget. Two ticks over Stack's two surfaces. | runtime/src/surface.c:341-417 at e239e1c^ (the row walk of loom_surface_build_frame) with runtime/src/frame-build.c:158-174 at e239e1c^ (loom_frame_build_add_dma), taking the LoomSurfaceFlush record surface.c fills at e239e1c | runtime/backends/pvsneslib/src/board.asm:loom_pvs_surface_flush (+ loom_pvs_flush_spec, loom_pvs_board_mul) at e239e1c (unchanged at HEAD) |
| `witness_hash` | e239e1c | Board hash for the debug witness: the sum of cell * (2 * index + 1) over 90 cells of two 10x20 boards (a whole board runs past a frame through 816-tcc). | runtime/src/board.c at e239e1c (the host #else branch of loom_board_hash: the weighted-sum loop); at e239e1c^ (board.c:523-541) the hash was djb2, which the same commit replaced | runtime/backends/pvsneslib/src/board.asm:loom_pvs_board_hash at e239e1c (unchanged at HEAD) |

### Micro benchmarks (unit.c only)

| bench | what |
|---|---|
| `micro_calls` | Calls: a chain of small static functions (7 calls deep per chain), 9 mixed-width arguments, pointer out-parameters, and 12 calls through a const table of function pointers. |
| `micro_index` | Array indexing: map[y][x] over a 16x12 u8 map through a const table, a 2-D u16 relaxation sweep, a histogram hist[a[i] >> 4]++, an in-place u16 reverse. |
| `micro_loops` | Loops: sum u16/u8 arrays (indexed and pointer-walk), memset-like u8/u16 fills, a byte copy, a triangular nested loop. |
| `micro_math` | 16-bit math: a clamped sub-pixel axis step, signed/unsigned compare mixes, an LFSR, popcount, sum of absolute differences, 8-bit saturating add, signed-char sum. |
| `micro_muldiv` | Multiply by constants and variables (signed and unsigned), divide/modulo by constants (/16, /10 digit loop, /3, %7, signed /4 /3 %5) and by variables (signed and unsigned). |
| `micro_struct` | Struct access in 816-tcc layouts: a[i].f over a 20-byte struct with a pointer field, the same walk by pointer through ->def, a linked list, struct copy and return by value. |
| `micro_switch` | switch: a dense 12-case switch (13 calls), a sparse 8-case switch over scattered 16-bit keys (16 calls), a 3-state tokenizer over a 37-byte string. |

### Findings made while building the pairs

- **16-bit unsigned promotion in Loom's C (body_scan_floor, body_scan_ceiling).**
  `floor >= feet - LOOM_MOVEMENT_TILE_PIXELS` and `row_top + 15 >= reach`
  compare unsigned under 816-tcc (the constant is a `loom_u16` and `int` is
  16 bits), while the asm (and a 32-bit host) compare signed. They disagree only
  for feet < 16 / negative reach at a room's top edge; the inputs avoid that.
- **A bug in the asm at 7b4d960 (oam_batch).** `loom_pvs_oam_batch_accept`
  stores `current[slot] = slot` instead of `current[count] = slot` (fixed in
  Loom 8452f6d); the inputs number slots in staging order so both agree.
- **board_fill** agrees only when every cell changes (the C redraws whole rows,
  the asm only changed cells); **witness_hash** uses the replacement's own
  host C (the commit changed the hash function while moving it to asm).
- Some pairs needed glue that is not Loom code (player_sprite's entry,
  solid_actor_query's ABI entries: about a dozen instructions a call on the asm
  side) or stubs in driver.c (actor_body_step's tile probes, player_tick's
  actor hooks); each bench.toml says so.
- 816-tcc is slow enough that the one-frame limit forced small workloads
  (e.g. board_paint writes 3 cells, witness_hash hashes 90 cells).
