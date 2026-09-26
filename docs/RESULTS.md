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

(Geometric means. Lower is better; asm/tcc clocks is 0.32 for scale.)


## Side by side: where the remaining gap comes from

### movement_box_blocked (Loom f0a6cd0): 816-tcc 279,504 clocks, loomcc 163,112, hand asm 95,936

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

loomcc (12 instructions per cell): values live in direct page, the compare
branches directly, the byte is read through `[dp],y`:

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

Hand assembly (5 instructions per cell): the loop counter *is* Y, the loop runs
in 8-bit accumulator mode, and the exit test falls through:

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

The remaining gap, in order of size:

1. **No register allocation to X/Y.** The hand code keeps the induction
   variable in Y (`iny`, `cpy`); loomcc keeps it in a direct-page word and
   reloads it (`lda $0c; inc a; sta $0c` + `ldy $0c`).
2. **No loop optimisation.** The hand code hoists the row multiply out of the
   loop and strength-reduces it to an add per row; loomcc calls its multiply
   helper once per row (the C does the multiply there).
3. **Block layout and jump threading.** `bne solid; bra __b17` then `__b17`
   jumps to `__b14`: two taken branches per cell that a layout pass removes.
4. **8-bit regions.** The hand loop reads the byte in 8-bit mode (`lda [r5],y;
   beq`) instead of `lda; and #$ff; bne`.
5. **Argument traffic.** The 816-tcc ABI entry copies each stack argument to
   the static frame and the body copies it again into direct page; the hand
   code reads its arguments with `lda n,s` where they are.

The same five items account for most of the gap in the other pairs
(player_tick, camera_follow and board_* also pay for copies of pointers that
the IR does not yet propagate).

### Why loomcc is already 1.85x faster than 816-tcc

- values in direct page instead of stack reloads (`lda.b $0c` 4 cycles vs
  `lda n,s` + `sta r0` + reload);
- compare-and-branch fused (`cmp; bcc`) instead of a 0/1 materialised in X;
- `a[i].f` as one indexed access (`lda arr+off,x`, `[dp],y`) instead of a
  `tcc__mul` call and a 4-byte pointer built per access;
- multiplication by constants as shifts and adds;
- internal calls write arguments straight into the callee's frame (no
  pushes, no pops, no frame set-up);
- `x == 0` as `lda; beq` (2 instructions instead of ~11).

## External test suite (loomcc-tests)

| run | t1-pp | t2-parse | t3-sema | t4-exec | t5-snes | t6-loom | t7-random | total pass |
|---|---|---|---|---|---|---|---|---|
| M5 (2026-09-26) | 379/458 | 75/79 | 77/86 | 220/234 | 17/17 | - | - | 767/874 |
| M6c + correctness fixes (F17, F18, loop, Q1, Q2, diagnostics) | 448/458 | 79/79 | 85/87 | 299/316 | 17/17 | 31/31 | 80/80 | 1039/1068 |

Remaining t4-exec failures are only the unsupported features (32-bit
multiply/divide/variable shifts, recursion); t1 failures are `__VA_OPT__`
details, one deferred-rescan hide-set case, and two mcpp edge diagnostics.

## Current table (M5 + fixes)

| bench | kind | equal | tcc bytes | tcc instr | tcc clocks | asm bytes | asm instr | asm clocks | loomcc bytes | loomcc instr | loomcc clocks | loomcc/tcc clocks | loomcc/asm clocks | cstack bytes |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| actor_body_step | pair | yes | 3613 | 5571 | 243808 | 1331 | 2033 | 161492 | 2223 | 3112 | 190292 | 0.78 | 1.18 | 24 |
| animation_pass | pair | yes | 404 | 6031 | 214404 | 307 | 2913 | 120660 | 304 | 3064 | 107676 | 0.50 | 0.89 | 16 |
| board_drop | pair | yes | 3733 | 10585 | 246772 | 1011 | 1842 | 51536 | 2344 | 6884 | 175752 | 0.71 | 3.41 | 76 |
| board_fill | pair | yes | 2477 | 8511 | 222912 | 558 | 2308 | 64244 | 1567 | 4635 | 135636 | 0.61 | 2.11 | 48 |
| board_fit | pair | yes | 568 | 10404 | 232208 | 346 | 2232 | 66228 | 384 | 6552 | 151776 | 0.65 | 2.29 | 22 |
| board_paint | pair | yes | 3061 | 7953 | 188976 | 746 | 1381 | 39052 | 1936 | 4940 | 127376 | 0.67 | 3.26 | 64 |
| body_probe_x | pair | yes | 739 | 6813 | 154716 | 242 | 2082 | 51572 | 495 | 3561 | 90492 | 0.58 | 1.75 | 16 |
| body_scan_ceiling | pair | yes | 422 | 5533 | 127932 | 186 | 2037 | 48848 | 253 | 2843 | 69520 | 0.54 | 1.42 | 8 |
| body_scan_floor | pair | yes | 979 | 9451 | 213520 | 301 | 3146 | 75280 | 562 | 4563 | 112772 | 0.53 | 1.50 | 20 |
| camera_follow | pair | yes | 4021 | 9570 | 239684 | 1436 | 2087 | 66584 | 2575 | 4570 | 136524 | 0.57 | 2.05 | 50 |
| contact_scan | pair | yes | 802 | 7727 | 195316 | 390 | 2608 | 89748 | 479 | 3462 | 107364 | 0.55 | 1.20 | 38 |
| display_block | pair | yes | 948 | 4575 | 123584 | 186 | 844 | 27608 | 432 | 1829 | 54640 | 0.44 | 1.98 | 14 |
| layer_scroll | pair | yes | 1871 | 8555 | 214524 | 506 | 2555 | 77696 | 1392 | 4863 | 144028 | 0.67 | 1.85 | 36 |
| movement_box_blocked | pair | yes | 375 | 11201 | 279504 | 157 | 3485 | 95936 | 285 | 5760 | 163112 | 0.58 | 1.70 | 18 |
| oam_batch | pair | yes | 2651 | 9020 | 240604 | 515 | 1496 | 35196 | 1530 | 4645 | 126192 | 0.52 | 3.59 | 34 |
| oam_finish | pair | yes | 654 | 5320 | 128864 | 181 | 1087 | 24852 | 333 | 2570 | 64608 | 0.50 | 2.60 | 0 |
| oam_stage | pair | yes | 1292 | 7484 | 186392 | 355 | 1934 | 50832 | 582 | 2973 | 81760 | 0.44 | 1.61 | 4 |
| player2_position | pair | yes | 287 | 799 | 20512 | 127 | 282 | 11952 | 137 | 349 | 13116 | 0.64 | 1.10 | 8 |
| player_sprite | pair | yes | 545 | 2928 | 67216 | 236 | 711 | 25288 | 406 | 1189 | 34396 | 0.51 | 1.36 | 30 |
| player_tick | pair | yes | 8995 | 8135 | 200464 | 2750 | 2606 | 81004 | 5461 | 4115 | 117400 | 0.59 | 1.45 | 58 |
| scene_trigger_scan | pair | yes | 386 | 8287 | 217380 | 145 | 2300 | 64964 | 229 | 4103 | 111852 | 0.51 | 1.72 | 10 |
| solid_actor_query | pair | yes | 1749 | 9750 | 230524 | 536 | 3549 | 111652 | 1116 | 4376 | 115152 | 0.50 | 1.03 | 38 |
| surface_flush | pair | yes | 2040 | 8420 | 217636 | 677 | 1727 | 49360 | 1342 | 4336 | 122116 | 0.56 | 2.47 | 58 |
| witness_hash | pair | yes | 126 | 8388 | 231036 | 72 | 2310 | 47956 | 100 | 3116 | 84460 | 0.37 | 1.76 | 6 |
| micro_calls | micro | yes | 872 | 2882 | 80444 | - | - | - | 757 | 1434 | 45188 | 0.56 | - | 20 |
| micro_index | micro | yes | 840 | 8803 | 220124 | - | - | - | 446 | 4529 | 101564 | 0.46 | - | 8 |
| micro_loops | micro | yes | 665 | 8637 | 234132 | - | - | - | 444 | 3497 | 90764 | 0.39 | - | 10 |
| micro_math | micro | yes | 1101 | 6406 | 155716 | - | - | - | 749 | 3168 | 79812 | 0.51 | - | 12 |
| micro_muldiv | micro | yes | 580 | 7877 | 185940 | - | - | - | 597 | 5326 | 140028 | 0.75 | - | 10 |
| micro_struct | micro | yes | 1620 | 6123 | 178672 | - | - | - | 952 | 2118 | 55516 | 0.31 | - | 20 |
| micro_switch | micro | yes | 1180 | 5899 | 140404 | - | - | - | 601 | 2484 | 64684 | 0.46 | - | 6 |

31 benchmarks, 31 with identical result words in every variant.

Geometric means, loomcc relative to 816-tcc (all benchmarks): clocks 0.54x, instructions 0.49x, bytes 0.63x.
Geometric means, loomcc relative to hand assembly (the 24 pairs): clocks 1.76x, instructions 1.84x, bytes 1.79x.
For scale, hand assembly relative to 816-tcc: clocks 0.32x.
Compiled-stack WRAM: largest single benchmark 76 bytes; sum over all 782 bytes.
