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

(Geometric means. Lower is better; asm/tcc clocks is 0.32 for scale.)

## Current table (phase M5)

| bench | kind | equal | tcc bytes | tcc instr | tcc clocks | asm bytes | asm instr | asm clocks | loomcc bytes | loomcc instr | loomcc clocks | loomcc/tcc clocks | loomcc/asm clocks | cstack bytes |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| actor_body_step | pair | yes | 3613 | 5571 | 243808 | 1331 | 2033 | 161492 | 2223 | 3112 | 190292 | 0.78 | 1.18 | 24 |
| animation_pass | pair | yes | 404 | 6031 | 214404 | 307 | 2913 | 120660 | 304 | 3064 | 107676 | 0.50 | 0.89 | 16 |
| board_drop | pair | yes | 3733 | 10585 | 246772 | 1011 | 1842 | 51536 | 2264 | 6820 | 173920 | 0.70 | 3.37 | 76 |
| board_fill | pair | yes | 2477 | 8511 | 222912 | 558 | 2308 | 64244 | 1487 | 4507 | 131936 | 0.59 | 2.05 | 48 |
| board_fit | pair | yes | 568 | 10404 | 232208 | 346 | 2232 | 66228 | 384 | 6552 | 151776 | 0.65 | 2.29 | 22 |
| board_paint | pair | yes | 3061 | 7953 | 188976 | 746 | 1381 | 39052 | 1856 | 4876 | 125544 | 0.66 | 3.21 | 64 |
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
| solid_actor_query | pair | yes | 1749 | 9750 | 230524 | 536 | 3549 | 111652 | 1100 | 4228 | 110888 | 0.48 | 0.99 | 38 |
| surface_flush | pair | yes | 2040 | 8420 | 217636 | 677 | 1727 | 49360 | 1335 | 4324 | 121716 | 0.56 | 2.47 | 58 |
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
Geometric means, loomcc relative to hand assembly (the 24 pairs): clocks 1.76x, instructions 1.84x, bytes 1.78x.
For scale, hand assembly relative to 816-tcc: clocks 0.32x.
Compiled-stack WRAM: largest single benchmark 76 bytes; sum over all 782 bytes.
