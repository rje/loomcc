# Baseline results

Measured 2026-09-25 with `testbed/harness/run_all.py` (816-tcc 0.9.25 + 816-opt
2.0.0, `-F` LoROM FastROM; loom-emulator at Loom 5599b9c). Every benchmark
printed identical result words in the host, tcc and (for pairs) asm variants.

- **bytes**: ROM bytes of the unit's sections (code + `.rodata`).
- **instr (unit+helper)**: instructions executed inside the unit's sections,
  plus inside the sections of its `helper_labels` (`tcc__mul`, `tcc__udiv`,
  `memcpy`, ...), for the whole `bench_run`.
- **net clocks**: master clocks between the two H/V latches, minus the
  harness's 480-clock overhead. It includes the driver's call sequences
  (argument pushes, `jsl`, pops), identical in every variant. One frame is
  357368 master clocks.

| bench | words equal | tcc bytes | tcc instr (unit+helper) | tcc net clocks | asm bytes | asm instr (unit+helper) | asm net clocks |
|---|---|---:|---:|---:|---:|---:|---:|
| actor_body_step | yes | 3613 | 5571+0 | 243808 | 1331 | 2033+0 | 161492 |
| animation_pass | yes | 404 | 6031+0 | 214404 | 307 | 2913+0 | 120660 |
| board_drop | yes | 3733 | 9537+1048 | 246772 | 1011 | 1842+0 | 51536 |
| board_fill | yes | 2477 | 6959+1552 | 222912 | 558 | 2308+0 | 64244 |
| board_fit | yes | 568 | 9796+608 | 232208 | 346 | 2232+0 | 66228 |
| board_paint | yes | 3061 | 6975+978 | 188976 | 746 | 1381+0 | 39052 |
| body_probe_x | yes | 739 | 6813+0 | 154716 | 242 | 2082+0 | 51572 |
| body_scan_ceiling | yes | 422 | 5533+0 | 127932 | 186 | 2037+0 | 48848 |
| body_scan_floor | yes | 979 | 9451+0 | 213520 | 301 | 3146+0 | 75280 |
| camera_follow | yes | 4021 | 8223+1347 | 239684 | 1436 | 2087+0 | 66584 |
| contact_scan | yes | 802 | 7727+0 | 195316 | 390 | 2608+0 | 89748 |
| display_block | yes | 948 | 4575+0 | 123584 | 186 | 844+0 | 27608 |
| layer_scroll | yes | 1871 | 7256+1299 | 214524 | 506 | 2555+0 | 77696 |
| micro_calls | yes | 872 | 2726+156 | 80444 | - | - | - |
| micro_index | yes | 840 | 8803+0 | 220124 | - | - | - |
| micro_loops | yes | 665 | 8637+0 | 234132 | - | - | - |
| micro_math | yes | 1101 | 6406+0 | 155716 | - | - | - |
| micro_muldiv | yes | 580 | 1194+6683 | 185940 | - | - | - |
| micro_struct | yes | 1620 | 3684+2439 | 178672 | - | - | - |
| micro_switch | yes | 1180 | 5899+0 | 140404 | - | - | - |
| movement_box_blocked | yes | 375 | 9943+1258 | 279504 | 157 | 2745+740 | 95936 |
| oam_batch | yes | 2651 | 9020+0 | 240604 | 515 | 1496+0 | 35196 |
| oam_finish | yes | 654 | 5320+0 | 128864 | 181 | 1087+0 | 24852 |
| oam_stage | yes | 1292 | 7484+0 | 186392 | 355 | 1934+0 | 50832 |
| player2_position | yes | 287 | 799+0 | 20512 | 127 | 282+0 | 11952 |
| player_sprite | yes | 545 | 2928+0 | 67216 | 236 | 711+0 | 25288 |
| player_tick | yes | 8995 | 7958+177 | 200464 | 2750 | 2606+0 | 81004 |
| scene_trigger_scan | yes | 386 | 8287+0 | 217380 | 145 | 2300+0 | 64964 |
| solid_actor_query | yes | 1749 | 9750+0 | 230524 | 536 | 3549+0 | 111652 |
| surface_flush | yes | 2040 | 7492+928 | 217636 | 677 | 1727+0 | 49360 |
| witness_hash | yes | 126 | 4384+4004 | 231036 | 72 | 2310+0 | 47956 |

Over the 24 pairs, 816-tcc against the hand assembly (geometric means): **2.90x the bytes, 3.72x the instructions (unit+helper), 3.17x the net clocks** (clocks include the shared driver call sequences, so they understate the gap).
