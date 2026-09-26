# loomcc results

All numbers come from `testbed/harness` (see testbed/bench/README.md): one ROM per
benchmark and variant, the unit under test called through the 816-tcc ABI by an
816-tcc-compiled driver, run in loom-emulator (MesenCore).

- **bytes**: the unit's code + read-only data (hand asm: its sections; loomcc: every
  section it emitted, including its helpers).
- **instr**: instructions executed inside the unit's labels (and helpers it calls)
  during `bench_run`, from `loom-emulator trace --profile`.
- **clocks**: master clocks between the two H/V-counter latches around `bench_run`,
  minus the harness's own 480-clock overhead. They include the driver's 816-tcc call
  sequences (argument pushes, `jsl`, pops), which every variant pays alike, so the
  clock ratios understate the differences inside the units.
- **equal**: host (clang), tcc, asm and loomcc produced the same result words.
- **cstack bytes**: WRAM loomcc reserves for static frames (the compiled stack).

Variants: **tcc** = 816-tcc 0.9.25 + 816-opt (Loom's pipeline), **asm** = the hand-written
assembly that replaced the C in Loom's history, **loomcc** = this compiler.

## Progress by phase

| phase | loomcc/tcc clocks | loomcc/asm clocks (pairs) | loomcc/tcc bytes | equal |
|---|---:|---:|---:|---:|
| M5: first backend, no optimiser | 0.54 | 1.76 | 0.63 | 31/31 |
| M5 + correctness fixes from loomcc-tests (F11-F16) | 0.54 | 1.76 | 0.63 | 31/31 |
| M6a: IR clean-up (CFG simplify, copy propagation, def retargeting, DCE) | 0.51 | 1.68 | 0.62 | 31/31 |
| M6b: induction variables and indexes homed in X/Y | 0.50 | 1.66 | 0.62 | 31/31 |
| M6c: interprocedural direct page (a call clobbers only its callee tree's words), arguments written straight into callee homes | 0.45 | 1.48 | 0.51 | 31/31 |
| M6d: CPU multiplier/divider in the 16-bit helpers (software versions for interrupt context) | 0.42 | 1.41 | 0.54 | 31/31 |
| M6e: block layout (greedy traces) | 0.42 | 1.39 | 0.53 | 31/31 |
| M6f: loads folded into the next instruction's operand (`sbc [dp],y`), no bank copy on pointer increments | 0.41 | 1.36 | 0.52 | 31/31 |
| M6g: lazy 8-bit accumulator sections (consecutive byte stores share one `sep`) | 0.41 | 1.36 | 0.52 | 31/31 |
| M6h: range analysis re-folds non-negative signed indexes into indexed modes | 0.40 | 1.35 | 0.51 | 31/31 |
| M7a: whole-program inlining (small callees; single-call statics) | 0.39 | 1.30 | 0.50 | 31/31 |
| M7b: loop-invariant code motion (incl. loads the loop cannot write) and induction-variable strength reduction (`y*width` per row becomes an add) | 0.38 | 1.27 | 0.51 | 31/31 |
| M7c: branch-free compare results, clean 8-bit values, join-aware layout | 0.38 | 1.27 | 0.50 | 31/31 |

(Geometric means. Lower is better; asm/tcc clocks is 0.32 for scale.)


## Side by side: where the remaining gap comes from

### movement_box_blocked (Loom f0a6cd0): 816-tcc 279,504 clocks, hand asm 95,936, loomcc 89,628 (M7)

The inner loop, `for (x = first_x; x <= last_x; ++x) if (row[x] != 0) return 1;`:

816-tcc reloads every value from the stack and materialises the compare as a
0/1 in X before branching (about 30 instructions per cell):

```
__local_11:
lda -10 + __locals + 1,s      ; x
sta.b tcc__r0
lda -4 + __locals + 1,s       ; last_x
sta.b tcc__r1
ldx #1
lda.b tcc__r0
sec
sbc.b tcc__r1
tay
beq ++
bcc ++
+ dex
...                           ; stx r5 / txa / bne / brl, then row[x] through a
                              ; freshly built 4-byte pointer, then x++ via the stack
```

loomcc at M5 (first backend, 12 instructions per cell: direct page, fused
compare-branch, but the counter reloaded and a jump chain):

```
__b12: lda.b $0c        ; x
       cmp.b $08        ; last_x
       beq __b13
       bcs __b15
__b13: ldy.b $0c
       lda [$00],y      ; row[x]
       and.w #$ff
       bne __b16        ; solid
       bra __b17        ; -> __b14 (a jump chain)
__b14: lda.b $0c
       inc a
       sta.b $0c
       bra __b12
```

loomcc at M7 (7 instructions per cell): the counter lives in Y (X/Y homes),
the loop is rotated (test at the bottom), and the row multiply is gone from
the loop (strength reduction turns `y * collision_width` into an add per
row, and the one remaining multiply uses the CPU multiplier):

```
__b9:  lda [$00],y      ; row[x]
       and.w #$ff
       beq __b10
       ...              ; return 1
__b10: iny
       cpy.b $08        ; x <= last_x
       bcc __b9
       beq __b9
```

Hand assembly (5 instructions per cell): Y as counter too, and the whole loop
runs in 8-bit accumulator mode so the byte test needs no `and #$ff`:

```
loom_pvs_movement_cell:
  cpy.b tcc__r2
  beq loom_pvs_movement_read
  bcs loom_pvs_movement_row_next
loom_pvs_movement_read:
  lda.b [tcc__r5],y
  beq loom_pvs_movement_next_cell
  brl loom_pvs_movement_solid
loom_pvs_movement_next_cell:
  iny
  bra loom_pvs_movement_cell
```

loomcc now beats the hand code here overall (fewer instructions: its outer
loop and bounds checks are tighter), while its inner loop still pays the
`and #$ff` and a two-branch `<=`.

### What closed the gap, M5 -> M7 (loomcc/asm clocks 1.76 -> 1.27)

| step | effect |
|---|---|
| IR clean-up (copy propagation, def retargeting, jump threading with loop rotation) | 1.76 -> 1.68 |
| interprocedural direct page: a call clobbers only its callee tree's DP words; arguments written straight into callee homes | 1.66 -> 1.48 (largest single step) |
| CPU multiplier/divider | 1.48 -> 1.41 |
| layout, load folding into operands, range analysis | 1.41 -> 1.35 |
| whole-program inlining | 1.35 -> 1.30 |
| LICM + strength reduction | 1.30 -> 1.27 |

### Where the remaining 1.27x comes from

1. **Algorithmic differences in the pairs.** Several hand routines are not the
   same algorithm as the C they replaced: oam_batch (2.3x) validates less per
   entry, board_drop/board_paint (2.1x) fuse the shape paint, the tilemap
   words and the dirty spans into one pass where the C calls helpers per
   cell. loomcc executes the C faithfully; no compiler closes these.
2. **8-bit loops.** Byte loops stay in 16-bit mode with `and #$ff` on each
   byte read; the hand code switches the whole loop to 8-bit. loomcc's 8-bit
   sections are local (consecutive byte stores share a `sep`) but do not yet
   span loops.
3. **Register pressure at calls to code outside the unit.** A call to
   816-tcc code or hand assembly clobbers every direct-page scratch word, so
   values live across it go to the static frame (bank $7E absolute, one
   cycle more per access and one byte more per instruction).
4. **Two-branch unsigned `<=`/`>`** (`bcc; beq`) where the hand code arranges
   the comparison the other way round.

## External test suite (loomcc-tests)

| run | t1-pp | t2-parse | t3-sema | t4-exec | t5-snes | t6-loom | t7-random | total pass |
|---|---|---|---|---|---|---|---|---|
| M5 (2026-09-26) | 379/458 | 75/79 | 77/86 | 220/234 | 17/17 | - | - | 767/874 |
| M6c + correctness fixes (F17, F18, loop, Q1, Q2, diagnostics) | 448/458 | 79/79 | 85/87 | 299/316 | 17/17 | 31/31 | 80/80 | 1039/1068 |
| M7c (suite has grown to 2422 tests) | 449/460 | 303/309 | 300/303 | 1191/1214 | 17/17 | 39/39 | 80/80 | 2379/2422 |

Remaining t4-exec failures are only the unsupported features (32-bit
multiply/divide/variable shifts, recursion); t1 failures are `__VA_OPT__`
details, one deferred-rescan hide-set case, and two mcpp edge diagnostics.

## M8: Cliffside built with loomcc (2026-09-26)

**The whole Cliffside release ROM builds with loomcc**: all 27 C units
(Loom's portable runtime, the pvsneslib backend C, the generated tables
and schedule, and the game's own hook) compiled as one program with
inlining across units, beside Loom's unchanged hand assembly (body.asm,
oam.asm, scene.asm, movement.asm, board.asm, vblank.asm) and PVSnesLib's
crt0/libc, linked with wlalink. No unit needed 816-tcc
(docs/results/m8/units-loomcc.json).

Method (testbed/loom-build): a scratch copy of examples/cliffside was
packaged by `loom-automation` (open_room + package_release); the runtime
sources were snapshotted at Loom ec0f2a8 plus the working-tree change
recorded in docs/results/m8/runtime-provenance.txt. `build.py` rebuilds the
ROM from that snapshot with Loom's exact pipeline: its 816-tcc build is
**byte-identical** to the ROM Loom itself packaged, so the loomcc build
differs only in the compiler. Nothing was written inside the Loom repo.

### Behaviour

A faster build reaches each tick in fewer frames (it boots 15 frames
sooner, and a tick that lagged no longer does), so the frame-timed script
presses buttons during different ticks. Like Loom's own scripts/tick-trace.py,
`tickcompare.py` converts the script to the tick clock (each span's buttons
held until the tick counter reaches the span's end) and compares:

| check | result |
|---|---|
| debug builds: the 108-byte witness `loom_project_debug_state` after every tick (all bytes but `tick_started` at 106) | **813 ticks, 0 differ** |
| debug builds: the sequence of distinct presented frames | **582 = 582, identical** |
| release builds: the sequence of distinct presented frames (tick counter `logical_tick_count` from the runtime state) | **583 = 583, identical** |

Frame-for-frame screenshot equality does not hold and cannot: in the
debug 816-tcc build 34 ticks take three frames where loomcc takes two, so
the same image is presented one frame later relative to the tick counter.
Every image and every witness byte agree once time is measured in ticks.

(Re-verified on the final ROMs after the M8 interop fixes: identical
results. Q2 is also confirmed under Loom's exact include order and flags:
`testbed/loom-build/q2-int32.c` includes `<snes.h>`, `<loom/runtime.h>`,
`<stdint.h>` and `<stddef.h>` and asserts `sizeof(int32_t) == 4`,
`sizeof(int16_t) == 2`, `sizeof(size_t) == 2`.)

### Speed (Loom's measure: release ROM, frames 400-1000 of scripts/full-speed/cliffside.json)

| | instructions a frame | waiting | instructions a tick |
|---|---:|---:|---:|
| 816-tcc (tick_frames 2, 300 ticks) | 7019 | 1377 | **11,284** |
| loomcc (tick_frames 2, 300 ticks) | 6263 | 2285 | **7,956** (-29.5%) |

The 60 Hz budget is about 8,000 instructions a tick: the loomcc build is
inside it.

At **tick_frames 1** (same sources, `tick_frames = 1`), lag frames counted by
`lag_frame_counter` ($7E0035):

| | lag frames in 400-1000 | lag frames, whole run (1626 frames) |
|---|---:|---:|
| 816-tcc | 82 (13.7%) | 195 |
| loomcc | **3 (0.5%)** | 23 |

Memory: loomcc's C code is 72,256 bytes against 816-tcc's 147,737 (0.49x);
read-only data 5,253 against 4,244; the compiled stack (static frames) takes
**312 bytes** of bank $7E WRAM.

### Where the gain came from (instructions a tick, by unit)

After inlining, a callee's code counts in the unit it was inlined into (so
camera.c's work shows up in its callers). Loom's hand assembly is
unchanged and costs the same in both builds; it is now 5,738 of the 7,956.
The C part went from 5,546 to 2,219 instructions a tick (2.5x fewer).

| unit | 816-tcc instr/tick | loomcc instr/tick | saved |
|---|---:|---:|---:|
| runtime/src/mode1.c | 1042 | 463 | 579 |
| .loom/generated/src/runtime_schedule.c | 783 | 368 | 415 |
| runtime/src/ui.c | 585 | 202 | 384 |
| runtime/backends/pvsneslib/src/runtime-adapter.c | 392 | 31 | 361 |
| runtime/src/scene.c | 515 | 157 | 358 |
| runtime/src/animation.c | 308 | 116 | 192 |
| runtime/src/movement.c | 314 | 125 | 189 |
| runtime/src/frame-shell.c | 245 | 78 | 167 |
| runtime/src/actor.c | 245 | 109 | 136 |
| runtime/src/combat.c | 208 | 77 | 131 |
| runtime/backends/pvsneslib/src/frame-transaction.c | 97 | 6 | 91 |
| runtime/src/audio.c | 79 | 1 | 78 |
| (other) | 267 | 212 | 55 |
| project/Code/Portable/cliffside.c | 87 | 32 | 55 |
| (asm / library) | 55 | 1 | 54 |
| runtime/src/camera.c | 54 | 0 | 54 |
| runtime/src/frame-build.c | 268 | 234 | 34 |
| .loom/generated/target/pvsneslib/assets.asm | 11 | 0 | 11 |
| runtime/src/adventure.c | 6 | 0 | 6 |
| runtime/src/game.c | 5 | 0 | 5 |
| runtime/backends/pvsneslib/src/vblank.asm | 234 | 234 | 0 |
| runtime/backends/pvsneslib/src/scene.asm | 241 | 241 | 0 |
| (loomcc helpers) | 0 | 1 | -1 |
| runtime/backends/pvsneslib/src/oam.asm | 1953 | 1959 | -6 |
| runtime/backends/pvsneslib/src/startup.c | 0 | 6 | -6 |
| runtime/backends/pvsneslib/src/body.asm | 3289 | 3304 | -15 |
| **total (excluding waits)** | **11284** | **7957** | **3327** |


## Current table (M7c)

| bench | kind | equal | tcc bytes | tcc instr | tcc clocks | asm bytes | asm instr | asm clocks | loomcc bytes | loomcc instr | loomcc clocks | loomcc/tcc clocks | loomcc/asm clocks | cstack bytes |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| actor_body_step | pair | yes | 3613 | 5571 | 243808 | 1331 | 2033 | 161492 | 2112 | 3011 | 188200 | 0.77 | 1.17 | 24 |
| animation_pass | pair | yes | 404 | 6031 | 214404 | 307 | 2913 | 120660 | 207 | 2479 | 91768 | 0.43 | 0.76 | 16 |
| board_drop | pair | yes | 3733 | 10585 | 246772 | 1011 | 1842 | 51536 | 1787 | 4839 | 110120 | 0.45 | 2.14 | 54 |
| board_fill | pair | yes | 2477 | 8511 | 222912 | 558 | 2308 | 64244 | 1057 | 3236 | 90160 | 0.40 | 1.40 | 22 |
| board_fit | pair | yes | 568 | 10404 | 232208 | 346 | 2232 | 66228 | 325 | 4838 | 97564 | 0.42 | 1.47 | 10 |
| board_paint | pair | yes | 3061 | 7953 | 188976 | 746 | 1381 | 39052 | 1408 | 3519 | 82672 | 0.44 | 2.12 | 42 |
| body_probe_x | pair | yes | 739 | 6813 | 154716 | 242 | 2082 | 51572 | 429 | 2930 | 72056 | 0.47 | 1.40 | 14 |
| body_scan_ceiling | pair | yes | 422 | 5533 | 127932 | 186 | 2037 | 48848 | 215 | 2275 | 54684 | 0.43 | 1.12 | 8 |
| body_scan_floor | pair | yes | 979 | 9451 | 213520 | 301 | 3146 | 75280 | 512 | 3779 | 92476 | 0.43 | 1.23 | 22 |
| camera_follow | pair | yes | 4021 | 9570 | 239684 | 1436 | 2087 | 66584 | 2152 | 3348 | 96432 | 0.40 | 1.45 | 28 |
| contact_scan | pair | yes | 802 | 7727 | 195316 | 390 | 2608 | 89748 | 305 | 2255 | 68188 | 0.35 | 0.76 | 16 |
| display_block | pair | yes | 948 | 4575 | 123584 | 186 | 844 | 27608 | 363 | 1619 | 47848 | 0.39 | 1.73 | 14 |
| layer_scroll | pair | yes | 1871 | 8555 | 214524 | 506 | 2555 | 77696 | 788 | 2648 | 75488 | 0.35 | 0.97 | 24 |
| movement_box_blocked | pair | yes | 375 | 11201 | 279504 | 157 | 3485 | 95936 | 284 | 3175 | 89628 | 0.32 | 0.93 | 18 |
| oam_batch | pair | yes | 2651 | 9020 | 240604 | 515 | 1496 | 35196 | 1059 | 3188 | 80060 | 0.33 | 2.27 | 4 |
| oam_finish | pair | yes | 654 | 5320 | 128864 | 181 | 1087 | 24852 | 268 | 1856 | 44904 | 0.35 | 1.81 | 0 |
| oam_stage | pair | yes | 1292 | 7484 | 186392 | 355 | 1934 | 50832 | 538 | 2736 | 74796 | 0.40 | 1.47 | 4 |
| player2_position | pair | yes | 287 | 799 | 20512 | 127 | 282 | 11952 | 107 | 269 | 10556 | 0.51 | 0.88 | 8 |
| player_sprite | pair | yes | 545 | 2928 | 67216 | 236 | 711 | 25288 | 280 | 1026 | 28344 | 0.42 | 1.12 | 30 |
| player_tick | pair | yes | 8995 | 8135 | 200464 | 2750 | 2606 | 81004 | 5271 | 3523 | 102356 | 0.51 | 1.26 | 46 |
| scene_trigger_scan | pair | yes | 386 | 8287 | 217380 | 145 | 2300 | 64964 | 173 | 2508 | 67748 | 0.31 | 1.04 | 10 |
| solid_actor_query | pair | yes | 1749 | 9750 | 230524 | 536 | 3549 | 111652 | 724 | 3101 | 77992 | 0.34 | 0.70 | 26 |
| surface_flush | pair | yes | 2040 | 8420 | 217636 | 677 | 1727 | 49360 | 915 | 3025 | 78356 | 0.36 | 1.59 | 28 |
| witness_hash | pair | yes | 126 | 8388 | 231036 | 72 | 2310 | 47956 | 143 | 2366 | 60276 | 0.26 | 1.26 | 6 |
| micro_calls | micro | yes | 872 | 2882 | 80444 | - | - | - | 498 | 959 | 30596 | 0.38 | - | 20 |
| micro_index | micro | yes | 840 | 8803 | 220124 | - | - | - | 379 | 2452 | 59104 | 0.27 | - | 8 |
| micro_loops | micro | yes | 665 | 8637 | 234132 | - | - | - | 265 | 2322 | 57248 | 0.24 | - | 10 |
| micro_math | micro | yes | 1101 | 6406 | 155716 | - | - | - | 705 | 2505 | 61468 | 0.39 | - | 12 |
| micro_muldiv | micro | yes | 580 | 7877 | 185940 | - | - | - | 591 | 2137 | 53932 | 0.29 | - | 10 |
| micro_struct | micro | yes | 1620 | 6123 | 178672 | - | - | - | 746 | 1543 | 44440 | 0.25 | - | 20 |
| micro_switch | micro | yes | 1180 | 5899 | 140404 | - | - | - | 519 | 2050 | 52864 | 0.38 | - | 6 |

31 benchmarks, 31 with identical result words in every variant.

Geometric means, loomcc relative to 816-tcc (all benchmarks): clocks 0.38x, instructions 0.35x, bytes 0.50x.
Geometric means, loomcc relative to hand assembly (the 24 pairs): clocks 1.27x, instructions 1.37x, bytes 1.43x.
For scale, hand assembly relative to 816-tcc: clocks 0.32x.
Compiled-stack WRAM: largest single benchmark 54 bytes; sum over all 560 bytes.
