.include "hdr.asm"

.accu 16
.index 16
.16bit

; The platformer bodies' tile probes, in assembly: the wall probe along X,
; the floor scan under the feet and the ceiling scan over the head. The
; player's body and every actor's body run all three every tick, and
; through 816-tcc the three loops cost the player about 45 scanlines of
; the frame's 262. The portable C renditions stay in runtime/src/movement.c
; and runtime/src/actor.c under `#else`, exercised by the host suites; the
; two must agree exactly, and the tick-clock ROM tests pin the positions
; that hold them together.
;
; All three read the active grid through loom_movement_grid, a C struct in
; movement.h whose layout is spelled out here: cells (four-byte pointer)
; at 0, pixel_width (s16) at 4, pixel_height (s16) at 6, row_offsets (u16
; per tile row) from 8. movement.c asserts the sizes; a field moved there
; without this file following makes every lookup read the wrong cell.
;
; 816-tcc ABI: JSL/RTL, the first argument at 4,s (5,s after our php),
; 16-bit results in tcc__r0. Direct page is zero, so tcc__r0..r5 and
; tcc__r9/r10 are scratch; nothing of the caller's lives in them.
;
; Cells: 0 none, 1 solid, 2 one-way, 3 slope up right, 4 slope up left.

.BASE $00
.RAMSECTION "loom.pvs.actor.body.state" BANK $7e SLOT 2
loom_pvs_actor_step_ptr dsb 4
loom_pvs_actor_body_ptr dsb 4
loom_pvs_actor_intent dsb 2
loom_pvs_actor_grounded dsb 2
loom_pvs_actor_flags dsb 2
loom_pvs_actor_next dsb 2
loom_pvs_actor_next_sub dsb 2
loom_pvs_actor_delta dsb 2
loom_pvs_actor_edge dsb 2
loom_pvs_actor_wall_bottom dsb 2
; The pool's arrays, bound once a tick (four-byte pointers), the actor's
; index (and index * 2), and a step record of our own for the bound step.
loom_pvs_actor_arr_x dsb 4
loom_pvs_actor_arr_y dsb 4
; The solid-actor queries' own binding and scratch (loom_pvs_solid_bind).
loom_pvs_solid_slots dsb 4
loom_pvs_solid_visible dsb 4
loom_pvs_sq_left dsb 2
loom_pvs_sq_right dsb 2
loom_pvs_sq_top dsb 2
loom_pvs_sq_bottom dsb 2
loom_pvs_sq_pos dsb 2
loom_pvs_sq_idx dsb 2
loom_pvs_sq_found dsb 2
loom_pvs_sq_best dsb 2
loom_pvs_sq_x dsb 2
loom_pvs_sq_y dsb 2
loom_pvs_sq_type dsb 4
loom_pvs_actor_arr_sub_x dsb 4
loom_pvs_actor_arr_sub_y dsb 4
loom_pvs_actor_arr_vx dsb 4
loom_pvs_actor_arr_vy dsb 4
loom_pvs_actor_arr_intent_x dsb 4
loom_pvs_actor_arr_intent_y dsb 4
loom_pvs_actor_arr_body_flags dsb 4
loom_pvs_actor_arr_riding dsb 4
loom_pvs_actor_idx dsb 2
loom_pvs_actor_idx2 dsb 2
loom_pvs_actor_record dsb 44
loom_pvs_actor_intent_y dsb 2
loom_pvs_actor_jumped dsb 2
loom_pvs_actor_reach dsb 2
loom_pvs_actor_landed dsb 2
loom_pvs_actor_stopped dsb 2
loom_pvs_actor_bound dsb 2
loom_pvs_actor_probe dsb 2
loom_pvs_actor_row dsb 2
loom_pvs_actor_tmp dsb 2
loom_pvs_player_held dsb 2
loom_pvs_player_pressed dsb 2
loom_pvs_player_cap dsb 2
loom_pvs_player_speed dsb 2
loom_pvs_player_extended dsb 2
loom_pvs_player_running dsb 2
loom_pvs_player_jumped dsb 2
loom_pvs_player_col dsb 2
loom_pvs_player_climb dsb 2
loom_pvs_probe_result dsb 2
loom_pvs_actor_idx4 dsb 2
loom_pvs_actor_arr_alive dsb 4
loom_pvs_actor_arr_inert dsb 4
loom_pvs_actor_arr_awake dsb 4
loom_pvs_actor_arr_type_ptr dsb 4
loom_pvs_actor_arr_body_ptr dsb 4
loom_pvs_actor_arr_behavior_a dsb 4
loom_pvs_actor_arr_behavior_b dsb 4
loom_pvs_actor_arr_blocked dsb 4
loom_pvs_actor_arr_sprite_index dsb 4
loom_pvs_actor_arr_sent_x dsb 4
loom_pvs_actor_arr_sent_y dsb 4
loom_pvs_actor_arr_animation_sent dsb 4
loom_pvs_actor_instances dsb 4
loom_pvs_actor_waypoints dsb 4
loom_pvs_mode1_world_x dsb 4
loom_pvs_mode1_world_y dsb 4
loom_pvs_mode1_part_count dsb 4
loom_pvs_mode1_sprite_count dsb 2
loom_pvs_pass_count dsb 2
loom_pvs_pass_left dsb 2
loom_pvs_pass_top dsb 2
loom_pvs_pass_right dsb 2
loom_pvs_pass_bottom dsb 2
loom_pvs_pass_slow dsb 4
loom_pvs_pass_out dsb 4
loom_pvs_pass_slow_count dsb 2
loom_pvs_pass_sleeping dsb 2
loom_pvs_pass_stepped dsb 2
loom_pvs_pass_tmp dsb 2
loom_pvs_pass_cursor dsb 2
loom_pvs_pass_remaining dsb 2
loom_pvs_pass_airv dsb 2
loom_pvs_pass_ix dsb 2
loom_pvs_pass_iy dsb 2
loom_pvs_pass_keyv dsb 2
loom_pvs_pass_behavior dsb 2
loom_pvs_pass_topdown dsb 2
loom_pvs_td_x dsb 2
loom_pvs_td_x_before dsb 2
loom_pvs_td_y_before dsb 2
loom_pvs_td_y dsb 2
loom_pvs_td_pos dsb 2
loom_pvs_td_sub dsb 2
loom_pvs_td_other dsb 2
loom_pvs_td_other_sub dsb 2
loom_pvs_td_next dsb 2
loom_pvs_td_next_sub dsb 2
loom_pvs_td_dir dsb 2
loom_pvs_td_left dsb 2
loom_pvs_td_top dsb 2
loom_pvs_td_right dsb 2
loom_pvs_td_bottom dsb 2
loom_pvs_td_row dsb 2
loom_pvs_td_col dsb 2
loom_pvs_combat_shot dsb 4
loom_pvs_mode1_slot_index dsb 4
loom_pvs_mode1_camera dsb 4
loom_pvs_mode1_cmin_x dsb 2
loom_pvs_mode1_cmin_y dsb 2
loom_pvs_mode1_cmax_x dsb 2
loom_pvs_mode1_cmax_y dsb 2
loom_pvs_cam_target_x dsb 2
loom_pvs_cam_target_y dsb 2
loom_pvs_cam_regions_left dsb 2
loom_pvs_cam_focus dsb 2
loom_pvs_cam_tmp dsb 2
loom_pvs_cam_axis_camera dsb 2
loom_pvs_cam_axis_anchor dsb 2
loom_pvs_cam_axis_origin dsb 2
loom_pvs_cam_axis_page dsb 2
loom_pvs_cam_axis_dz dsb 2
loom_pvs_cam_axis_target dsb 2
loom_pvs_cam_low dsb 2
loom_pvs_cam_high dsb 2
loom_pvs_cam_goal_x dsb 2
loom_pvs_cam_goal_y dsb 2
loom_pvs_cam_frac dsb 2
loom_pvs_cam_part dsb 2
loom_pvs_cam_whole dsb 2
loom_pvs_anim_playing dsb 4
loom_pvs_anim_elapsed dsb 4
loom_pvs_anim_frame_index dsb 4
loom_pvs_anim_frames dsb 4
.ENDS
.BASE LOOM_ROM_BASE

.SECTION "loom.pvs.body.code" SUPERFREE KEEP

; Signed 16-bit compare, A against the operand: leaves N set when A is
; less, as the C's signed compare would. Clobbers A.
.MACRO LOOM_SIGNED_CMP ARGS operand
  sec
  sbc.b operand
  bvc _skip\@
  eor #$8000
_skip\@:
.ENDM

; The same against a word in RAM by long address.
.MACRO LOOM_SIGNED_CMP_L ARGS operand
  sec
  sbc.l operand
  bvc _skipl\@
  eor #$8000
_skipl\@:
.ENDM

; The same against the word at [pointer],y.
.MACRO LOOM_SIGNED_CMP_IND ARGS pointer
  sec
  sbc [pointer],y
  bvc _skipi\@
  eor #$8000
_skipi\@:
.ENDM

; The cell at (X, A): X the pixel column, A the pixel row. Returns the cell
; in A (16-bit, 0..4), solid outside the room. Clobbers X, Y, tcc__r0,
; tcc__r10; the cells pointer must already sit in tcc__r9/tcc__r9h.
loom_pvs_body_cell:
  rep #$30
  bmi loom_pvs_body_cell_solid          ; y < 0
  cmp.l loom_movement_grid+6
  bcs loom_pvs_body_cell_solid          ; y >= pixel_height
  sta.b tcc__r0
  txa
  bmi loom_pvs_body_cell_solid          ; x < 0
  cmp.l loom_movement_grid+4
  bcs loom_pvs_body_cell_solid          ; x >= pixel_width
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r10                        ; x >> 4
  lda.b tcc__r0
  lsr a
  lsr a
  lsr a
  and #$fffe                            ; (y >> 4) * 2
  tax
  lda.l loom_movement_grid+8,x          ; row_offsets[y >> 4]
  clc
  adc.b tcc__r10
  tay
  sep #$20
  lda [tcc__r9],y
  rep #$20
  and #$00ff
  rts
loom_pvs_body_cell_solid:
  rep #$30
  lda #1
  rts

; Loads the grid's cells pointer into tcc__r9/tcc__r9h.
loom_pvs_body_grid:
  rep #$30
  lda.l loom_movement_grid
  sta.b tcc__r9
  lda.l loom_movement_grid+2
  sta.b tcc__r9h
  rts

; loom_s16 loom_pvs_body_probe_x(loom_s16 edge,        ;  5,s
;                                loom_s16 delta,       ;  7,s (nonzero)
;                                loom_s16 top,         ;  9,s
;                                loom_s16 wall_bottom) ; 11,s
; The first probe column from edge + sign through edge + delta (stepping
; by whole tiles after the first) with a solid cell in rows top through
; wall_bottom, or $7fff when none.
; Scratch: r1 probe, r2 bound, r3 top, r4 wall_bottom, r5 the row.
loom_pvs_body_probe_x:
  php
  rep #$30
  jsr loom_pvs_body_grid
  lda 5,s
  clc
  adc 7,s
  sta.b tcc__r2                         ; bound = edge + delta
  lda 9,s
  sta.b tcc__r3
  lda 11,s
  sta.b tcc__r4
  lda 7,s
  bmi loom_pvs_body_probe_x_left
  lda 5,s
  inc a
  sta.b tcc__r1                         ; probe = edge + 1
loom_pvs_body_probe_x_right_loop:
  ; while probe <= bound
  lda.b tcc__r1
  LOOM_SIGNED_CMP tcc__r2
  beq loom_pvs_body_probe_x_right_test
  bpl loom_pvs_body_probe_x_none
loom_pvs_body_probe_x_right_test:
  jsr loom_pvs_body_probe_x_column
  bcs loom_pvs_body_probe_x_hit
  lda.b tcc__r1
  and #$000f
  cmp #$000f
  beq +
  lda.b tcc__r1
  ora #$000f
  sta.b tcc__r1
+ inc.b tcc__r1
  bra loom_pvs_body_probe_x_right_loop
loom_pvs_body_probe_x_left:
  lda 5,s
  dec a
  sta.b tcc__r1                         ; probe = edge - 1
loom_pvs_body_probe_x_left_loop:
  ; while probe >= bound
  lda.b tcc__r1
  LOOM_SIGNED_CMP tcc__r2
  bmi loom_pvs_body_probe_x_none
  jsr loom_pvs_body_probe_x_column
  bcs loom_pvs_body_probe_x_hit
  lda.b tcc__r1
  and #$000f
  beq +
  lda.b tcc__r1
  and #$fff0
  sta.b tcc__r1
+ dec.b tcc__r1
  bra loom_pvs_body_probe_x_left_loop
loom_pvs_body_probe_x_none:
  lda #$7fff
  sta.b tcc__r0
  plp
  rtl
loom_pvs_body_probe_x_hit:
  lda.b tcc__r1
  sta.b tcc__r0
  plp
  rtl

; Column r1, rows r3 through r4 by tile: carry set when a cell is solid.
loom_pvs_body_probe_x_column:
  lda.b tcc__r3
  sta.b tcc__r5
- lda.b tcc__r5
  LOOM_SIGNED_CMP tcc__r4
  beq +
  bpl loom_pvs_body_probe_x_column_clear
+ ldx.b tcc__r1
  lda.b tcc__r5
  jsr loom_pvs_body_cell
  cmp #1
  beq loom_pvs_body_probe_x_column_solid
  lda.b tcc__r5
  ora #$000f
  inc a
  sta.b tcc__r5
  bra -
loom_pvs_body_probe_x_column_solid:
  sec
  rts
loom_pvs_body_probe_x_column_clear:
  clc
  rts

; loom_s16 loom_pvs_body_scan_floor(loom_s16 left,     ;  5,s
;                                   loom_s16 right,    ;  7,s
;                                   loom_s16 feet,     ;  9,s
;                                   loom_s16 reach,    ; 11,s
;                                   loom_s16 sensor_x) ; 13,s
; The first floor between the feet and the reach: a slope under the
; sensor decides its row (its surface, if it lies within a tile above the
; feet and not past the reach); otherwise a solid cell across the box, or
; a one-way cell whose row starts below the feet. -1 when there is none.
; Scratch: r1 row_top, r2 the column, r3 the floor, r4 right, r5 reach,
; r2h feet, r3h sensor_x, r4h feet - 16.
loom_pvs_body_scan_floor:
  php
  rep #$30
  jsr loom_pvs_body_grid
  lda 7,s
  sta.b tcc__r4
  lda 11,s
  sta.b tcc__r5
  lda 9,s
  sta.b tcc__r2h
  sec
  sbc #16
  sta.b tcc__r4h
  lda 13,s
  sta.b tcc__r3h
  lda 9,s
  inc a
  and #$fff0
  sta.b tcc__r1                         ; row_top = (feet + 1) & ~15
loom_pvs_body_scan_floor_row:
  lda.b tcc__r1
  LOOM_SIGNED_CMP tcc__r5
  beq +
  bmi +
  brl loom_pvs_body_scan_floor_none     ; row_top > reach
+ ldx.b tcc__r3h
  lda.b tcc__r1
  jsr loom_pvs_body_cell
  cmp #3
  bne +
  brl loom_pvs_body_scan_floor_slope_right
+ cmp #4
  bne +
  brl loom_pvs_body_scan_floor_slope_left
+
  ; the box's columns
  lda 5,s
  sta.b tcc__r2
loom_pvs_body_scan_floor_column:
  lda.b tcc__r2
  LOOM_SIGNED_CMP tcc__r4
  beq +
  bmi +
  brl loom_pvs_body_scan_floor_next     ; x > right
+ ldx.b tcc__r2
  lda.b tcc__r1
  jsr loom_pvs_body_cell
  cmp #1
  beq loom_pvs_body_scan_floor_lip
  cmp #2
  bne loom_pvs_body_scan_floor_column_next
  ; one-way: only when the feet are above the row
  lda.b tcc__r2h
  LOOM_SIGNED_CMP tcc__r1
  bmi loom_pvs_body_scan_floor_lip
loom_pvs_body_scan_floor_column_next:
  lda.b tcc__r2
  ora #$000f
  inc a
  sta.b tcc__r2
  bra loom_pvs_body_scan_floor_column
loom_pvs_body_scan_floor_lip:
  lda.b tcc__r1
  dec a
  sta.b tcc__r0
  plp
  rtl
loom_pvs_body_scan_floor_slope_right:
  ; floor = row_top + 15 - (sensor_x & 15)
  lda.b tcc__r3h
  and #$000f
  eor #$ffff
  sec
  adc.b tcc__r1                         ; row_top - (x & 15)
  clc
  adc #15
  bra loom_pvs_body_scan_floor_slope
loom_pvs_body_scan_floor_slope_left:
  lda.b tcc__r3h
  and #$000f
  clc
  adc.b tcc__r1
loom_pvs_body_scan_floor_slope:
  sta.b tcc__r3
  ; floor >= feet - 16 and floor <= reach
  lda.b tcc__r3
  LOOM_SIGNED_CMP tcc__r4h
  bpl +
  brl loom_pvs_body_scan_floor_next
+ lda.b tcc__r3
  LOOM_SIGNED_CMP tcc__r5
  beq +
  bmi +
  brl loom_pvs_body_scan_floor_next
+ lda.b tcc__r3
  sta.b tcc__r0
  plp
  rtl
loom_pvs_body_scan_floor_next:
  lda.b tcc__r1
  clc
  adc #16
  sta.b tcc__r1
  brl loom_pvs_body_scan_floor_row
loom_pvs_body_scan_floor_none:
  lda #$ffff
  sta.b tcc__r0
  plp
  rtl

; loom_s16 loom_pvs_body_scan_ceiling(loom_s16 left,  ;  5,s
;                                     loom_s16 right, ;  7,s
;                                     loom_s16 head,  ;  9,s
;                                     loom_s16 reach) ; 11,s
; The row under the first solid cell between the head and the reach
; above it (row_top + 16), or -1.
; Scratch: r1 row_top, r2 the column, r4 right, r5 reach.
loom_pvs_body_scan_ceiling:
  php
  rep #$30
  jsr loom_pvs_body_grid
  lda 7,s
  sta.b tcc__r4
  lda 11,s
  sta.b tcc__r5
  lda 9,s
  dec a
  and #$fff0
  sta.b tcc__r1                         ; row_top = (head - 1) & ~15
loom_pvs_body_scan_ceiling_row:
  lda.b tcc__r1
  clc
  adc #15
  LOOM_SIGNED_CMP tcc__r5
  bpl +
  brl loom_pvs_body_scan_ceiling_none   ; row_top + 15 < reach
+
  lda 5,s
  sta.b tcc__r2
loom_pvs_body_scan_ceiling_column:
  lda.b tcc__r2
  LOOM_SIGNED_CMP tcc__r4
  beq +
  bmi +
  brl loom_pvs_body_scan_ceiling_next
+ ldx.b tcc__r2
  lda.b tcc__r1
  jsr loom_pvs_body_cell
  cmp #1
  beq loom_pvs_body_scan_ceiling_hit
  lda.b tcc__r2
  ora #$000f
  inc a
  sta.b tcc__r2
  bra loom_pvs_body_scan_ceiling_column
loom_pvs_body_scan_ceiling_next:
  lda.b tcc__r1
  sec
  sbc #16
  sta.b tcc__r1
  bra loom_pvs_body_scan_ceiling_row
loom_pvs_body_scan_ceiling_hit:
  lda.b tcc__r1
  clc
  adc #16
  sta.b tcc__r0
  plp
  rtl
loom_pvs_body_scan_ceiling_none:
  lda #$ffff
  sta.b tcc__r0
  plp
  rtl


; ---------------------------------------------------------------------------
; The actor platformer body's step (runtime/src/actor.c,
; loom_actor_platformer_step) up to the floor merge: the ledge turn, the
; velocities, the X move against walls and the Y move's tile scan. The C
; caller fills a LoomActorBodyStep record, calls this, and finishes the step
; (the solid actors as floors, the slope under the sensor, the snap) from
; the record. Nothing here reads the actor pool, so its layout is free to
; change; the record's layout is this file's contract with actor.h:
;   x s16 @0, y @2, vx @4, vy @6, box_x @8, box_y @10, box_w u16 @12,
;   box_h @14, next_y @16, landed @18, stopped @20, reach @22, left @24,
;   right @26, top @28, bottom @30, sensor_x @32, sub_x u8 @34, sub_y @35,
;   next_sub_y @36, intent_x s8 @37, intent_y @38, grounded @39,
;   turn_at_ledges @40, flags @41, jumped @42, phase @43 (44 bytes).
; LoomPlatformerBody: max_speed @0, acceleration @2, friction @4,
;   air_control @6, gravity @8, terminal_velocity @10, jump_speed @12,
;   jump_cut @14 (u16 each).
; Flags: on ground 1, wall left 2, wall right 4, landed 8.

; (The scratch words live in loom.pvs.actor.body.state, declared above the
; code section: the helpers here are jsr targets, so everything sits in one
; bank.)

; r1 <- the step record pointer, r2 <- the body pointer.
loom_pvs_actor_pointers:
  rep #$30
  lda.l loom_pvs_actor_step_ptr
  sta.b tcc__r1
  lda.l loom_pvs_actor_step_ptr+2
  sta.b tcc__r1h
  lda.l loom_pvs_actor_body_ptr
  sta.b tcc__r2
  lda.l loom_pvs_actor_body_ptr+2
  sta.b tcc__r2h
  rts

; A <- the signed byte at [r1],y.
loom_pvs_actor_s8:
  sep #$20
  lda [tcc__r1],y
  rep #$20
  and #$00ff
  bit #$0080
  beq +
  ora #$ff00
+ cmp #0                                ; N and Z from the value, not the bit test
  rts

; whole (A) and sub (X, 0..255) advance by the velocity in r3: the
; arithmetic of LOOM_MOVEMENT_ADVANCE. Returns whole in A, sub in X.
loom_pvs_actor_advance:
  sta.b tcc__r4                         ; whole
  stx.b tcc__r5                         ; sub
  lda.b tcc__r3
  beq loom_pvs_actor_advance_done
  bmi loom_pvs_actor_advance_back
  and #$00ff
  clc
  adc.b tcc__r5                         ; c = sub + (v & 255)
  sta.b tcc__r5
  xba
  and #$00ff                            ; c >> 8
  sta.b tcc__r0
  lda.b tcc__r3
  xba
  and #$00ff                            ; v >> 8
  clc
  adc.b tcc__r0
  clc
  adc.b tcc__r4
  sta.b tcc__r4
  lda.b tcc__r5
  and #$00ff
  sta.b tcc__r5
  bra loom_pvs_actor_advance_done
loom_pvs_actor_advance_back:
  eor #$ffff
  inc a                                 ; m = -v
  sta.b tcc__r0
  and #$00ff                            ; f
  sta.b tcc__r3
  lda.b tcc__r5
  cmp.b tcc__r3
  bcs +                                 ; sub >= f: no borrow
  ; borrow: whole -= 1 more
  dec.b tcc__r4
+ lda.b tcc__r5
  sec
  sbc.b tcc__r3
  and #$00ff
  sta.b tcc__r5                         ; (sub - f) & 255
  lda.b tcc__r0
  xba
  and #$00ff                            ; m >> 8
  sta.b tcc__r0
  lda.b tcc__r4
  sec
  sbc.b tcc__r0
  sta.b tcc__r4
loom_pvs_actor_advance_done:
  lda.b tcc__r4
  ldx.b tcc__r5
  rts

; void loom_pvs_actor_body(LoomActorBodyStep *step, const LoomPlatformerBody *body)
loom_pvs_actor_body:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_actor_step_ptr
  lda 7,s
  sta.l loom_pvs_actor_step_ptr+2
  lda 9,s
  sta.l loom_pvs_actor_body_ptr
  lda 11,s
  sta.l loom_pvs_actor_body_ptr+2
  jsr loom_pvs_actor_core
  plp
  rtl

; The step on the record at loom_pvs_actor_step_ptr with the body at
; loom_pvs_actor_body_ptr; a subroutine, so the bound step below shares it.
loom_pvs_actor_core:
  jsr loom_pvs_actor_pointers
  ; intent, grounded, flags = 0
  ldy #37
  jsr loom_pvs_actor_s8
  sta.l loom_pvs_actor_intent
  ldy #39
  lda [tcc__r1],y
  and #$00ff
  sta.l loom_pvs_actor_grounded
  lda #0
  sta.l loom_pvs_actor_flags
  ; the box: left @24, top @28, right @26, bottom @30
  ldy #0
  lda [tcc__r1],y
  ldy #8
  clc
  adc [tcc__r1],y
  ldy #24
  sta [tcc__r1],y                       ; left
  ldy #12
  clc
  adc [tcc__r1],y
  dec a
  ldy #26
  sta [tcc__r1],y                       ; right
  ldy #2
  lda [tcc__r1],y
  ldy #10
  clc
  adc [tcc__r1],y
  ldy #28
  sta [tcc__r1],y                       ; top
  ldy #14
  clc
  adc [tcc__r1],y
  dec a
  ldy #30
  sta [tcc__r1],y                       ; bottom

  ; A ledge ahead is a wall to a walker that turns at them.
  lda.l loom_pvs_actor_intent
  bne _far1
  brl loom_pvs_actor_velocity_x
_far1:
  lda.l loom_pvs_actor_grounded
  bne _far2
  brl loom_pvs_actor_velocity_x
_far2:
  ldy #40
  lda [tcc__r1],y
  and #$00ff
  bne _far3
  brl loom_pvs_actor_velocity_x
_far3:
  lda.l loom_pvs_actor_intent
  bmi +
  ldy #26
  lda [tcc__r1],y                       ; right
  bra ++
+ ldy #24
  lda [tcc__r1],y                       ; left
++ clc
  adc.l loom_pvs_actor_intent           ; ahead
  tax
  jsr loom_pvs_body_grid                ; clobbers A, keeps X
  ldy #30
  lda [tcc__r1],y
  inc a                                 ; bottom + 1
  jsr loom_pvs_body_cell
  beq _far4
  brl loom_pvs_actor_velocity_x_reload
_far4:
  ; nothing under the step ahead: turn
  jsr loom_pvs_actor_pointers
  lda.l loom_pvs_actor_intent
  bmi +
  lda #4
  bra ++
+ lda #2
++ sta.l loom_pvs_actor_flags
  lda #0
  sta.l loom_pvs_actor_intent
  ldy #4
  sta [tcc__r1],y                       ; vx = 0
  brl loom_pvs_actor_velocity_x
loom_pvs_actor_velocity_x_reload:
  jsr loom_pvs_actor_pointers

loom_pvs_actor_velocity_x:
  ; Horizontal speed toward the intent, or friction on the ground.
  ldy #4
  lda [tcc__r1],y
  sta.b tcc__r3                         ; velocity
  lda.l loom_pvs_actor_intent
  bne _far5
  brl loom_pvs_actor_friction
_far5:
  lda.l loom_pvs_actor_grounded
  beq +
  ldy #2
  lda [tcc__r2],y                       ; acceleration
  bra ++
+ ldy #6
  lda [tcc__r2],y                       ; air_control
++ sta.b tcc__r4
  lda.l loom_pvs_actor_intent
  bmi +
  lda.b tcc__r3
  clc
  adc.b tcc__r4
  bra ++
+ lda.b tcc__r3
  sec
  sbc.b tcc__r4
++ sta.b tcc__r3
  ; clamp to +-max_speed
  ldy #0
  lda [tcc__r2],y
  sta.b tcc__r4                         ; max_speed
  lda.b tcc__r3
  LOOM_SIGNED_CMP tcc__r4
  beq +
  bmi +
  lda.b tcc__r4
  sta.b tcc__r3
  brl loom_pvs_actor_store_vx
+ lda.b tcc__r4
  eor #$ffff
  inc a
  sta.b tcc__r4                         ; -max_speed
  lda.b tcc__r3
  LOOM_SIGNED_CMP tcc__r4
  bmi _far6
  brl loom_pvs_actor_store_vx
_far6:
  lda.b tcc__r4
  sta.b tcc__r3
  brl loom_pvs_actor_store_vx
loom_pvs_actor_friction:
  lda.l loom_pvs_actor_grounded
  bne _far7
  brl loom_pvs_actor_store_vx
_far7:
  ldy #4
  lda [tcc__r2],y
  sta.b tcc__r4                         ; friction
  lda.b tcc__r3
  bne _far8
  brl loom_pvs_actor_store_vx
_far8:
  bmi +
  sec
  sbc.b tcc__r4
  bpl ++
  lda #0
  bra ++
+ clc
  adc.b tcc__r4
  bmi ++
  beq ++
  lda #0
++ sta.b tcc__r3
loom_pvs_actor_store_vx:
  lda.b tcc__r3
  ldy #4
  sta [tcc__r1],y

  ; Vertical: a controller's jump, its cut, gravity.
  ldy #6
  lda [tcc__r1],y
  sta.b tcc__r3                         ; velocity y
  lda #0
  ldy #42
  sep #$20
  sta [tcc__r1],y                       ; jumped = 0
  rep #$20
  ldy #38
  jsr loom_pvs_actor_s8                 ; intent_y
  bpl _far9
  brl loom_pvs_actor_jump
_far9:
  beq _far10
  brl loom_pvs_actor_gravity
_far10:
  ; intent_y == 0: cut a rise faster than jump_cut when jump_cut < jump_speed
  ldy #14
  lda [tcc__r2],y                       ; jump_cut
  sta.b tcc__r4
  ldy #12
  lda [tcc__r2],y                       ; jump_speed
  cmp.b tcc__r4
  bne _far11
  brl loom_pvs_actor_gravity
_far11:
  bcs _far12            ; jump_speed < jump_cut: no cut
  brl loom_pvs_actor_gravity
_far12:
  lda.b tcc__r4
  eor #$ffff
  inc a
  sta.b tcc__r4                         ; -jump_cut
  lda.b tcc__r3
  LOOM_SIGNED_CMP tcc__r4
  bmi _far13
  brl loom_pvs_actor_gravity
_far13:
  lda.b tcc__r4
  sta.b tcc__r3
  brl loom_pvs_actor_gravity
loom_pvs_actor_jump:
  lda.l loom_pvs_actor_grounded
  bne _far14
  brl loom_pvs_actor_gravity
_far14:
  ldy #12
  lda [tcc__r2],y
  eor #$ffff
  inc a
  sta.b tcc__r3                         ; -jump_speed
  lda #0
  sta.l loom_pvs_actor_grounded
  lda #1
  ldy #42
  sep #$20
  sta [tcc__r1],y                       ; jumped
  rep #$20
loom_pvs_actor_gravity:
  lda.l loom_pvs_actor_grounded
  beq _far15
  brl loom_pvs_actor_store_vy
_far15:
  ldy #8
  lda.b tcc__r3
  clc
  adc [tcc__r2],y                       ; + gravity
  sta.b tcc__r3
  ldy #10
  lda [tcc__r2],y
  sta.b tcc__r4                         ; terminal_velocity
  lda.b tcc__r3
  LOOM_SIGNED_CMP tcc__r4
  bne _far16
  brl loom_pvs_actor_store_vy
_far16:
  bpl _far17
  brl loom_pvs_actor_store_vy
_far17:
  lda.b tcc__r4
  sta.b tcc__r3
loom_pvs_actor_store_vy:
  lda.b tcc__r3
  ldy #6
  sta [tcc__r1],y

  ; X against solid cells, stepping the feet's half tile while grounded.
  ldy #4
  lda [tcc__r1],y
  sta.b tcc__r3                         ; vx
  ldy #34
  lda [tcc__r1],y
  and #$00ff
  tax                                   ; sub_x
  ldy #0
  lda [tcc__r1],y                       ; x
  jsr loom_pvs_actor_advance
  sta.l loom_pvs_actor_next
  txa
  sta.l loom_pvs_actor_next_sub
  lda.l loom_pvs_actor_next
  ldy #0
  cmp [tcc__r1],y
  bne _far18
  brl loom_pvs_actor_x_done
_far18:
  sec
  sbc [tcc__r1],y
  sta.l loom_pvs_actor_delta            ; delta = next - x
  bmi +
  ldy #26
  lda [tcc__r1],y                       ; right
  bra ++
+ ldy #24
  lda [tcc__r1],y                       ; left
++ sta.l loom_pvs_actor_edge
  ldy #30
  lda [tcc__r1],y                       ; bottom
  sta.l loom_pvs_actor_wall_bottom
  pha
  lda.l loom_pvs_actor_grounded
  tax
  pla
  cpx #0
  beq +
  sec
  sbc #8
  sta.l loom_pvs_actor_wall_bottom
  ldy #28
  LOOM_SIGNED_CMP_IND tcc__r1
  bpl +
  lda [tcc__r1],y                       ; wall_bottom < top: top
  sta.l loom_pvs_actor_wall_bottom
+ ; probe = loom_pvs_body_probe_x(edge, delta, top, wall_bottom)
  lda.l loom_pvs_actor_wall_bottom
  pha
  ldy #28
  lda [tcc__r1],y
  pha
  lda.l loom_pvs_actor_delta
  pha
  lda.l loom_pvs_actor_edge
  pha
  jsl loom_pvs_body_probe_x
  pla
  pla
  pla
  pla
  jsr loom_pvs_actor_pointers
  lda.b tcc__r0
  cmp #$7fff
  bne _far19
  brl loom_pvs_actor_x_store
_far19:
  ; stop = x + (probe - edge) - sign; a wall; vx = 0; sub = 0
  sec
  sbc.l loom_pvs_actor_edge
  ldy #0
  clc
  adc [tcc__r1],y
  sta.l loom_pvs_actor_next
  lda.l loom_pvs_actor_delta
  bmi +
  lda.l loom_pvs_actor_next
  dec a
  sta.l loom_pvs_actor_next
  lda.l loom_pvs_actor_flags
  ora #4
  sta.l loom_pvs_actor_flags
  bra ++
+ lda.l loom_pvs_actor_next
  inc a
  sta.l loom_pvs_actor_next
  lda.l loom_pvs_actor_flags
  ora #2
  sta.l loom_pvs_actor_flags
++ lda #0
  ldy #4
  sta [tcc__r1],y                       ; vx = 0
  sta.l loom_pvs_actor_next_sub
loom_pvs_actor_x_store:
  lda.l loom_pvs_actor_next
  ldy #0
  sta [tcc__r1],y                       ; x = stop
  ldy #8
  clc
  adc [tcc__r1],y
  ldy #24
  sta [tcc__r1],y                       ; left
  ldy #12
  clc
  adc [tcc__r1],y
  dec a
  ldy #26
  sta [tcc__r1],y                       ; right
loom_pvs_actor_x_done:
  lda.l loom_pvs_actor_next_sub
  ldy #34
  sep #$20
  sta [tcc__r1],y                       ; sub_x
  rep #$20
  ; sensor_x = left + box_w / 2
  ldy #12
  lda [tcc__r1],y
  lsr a
  ldy #24
  clc
  adc [tcc__r1],y
  ldy #32
  sta [tcc__r1],y

  ; Y: the move, then the floor scan or the ceiling scan.
  ldy #6
  lda [tcc__r1],y
  sta.b tcc__r3                         ; vy
  ldy #35
  lda [tcc__r1],y
  and #$00ff
  tax                                   ; sub_y
  ldy #2
  lda [tcc__r1],y                       ; y
  jsr loom_pvs_actor_advance
  ldy #16
  sta [tcc__r1],y                       ; next_y
  txa
  ldy #36
  sep #$20
  sta [tcc__r1],y                       ; next_sub_y
  rep #$20
  ldy #6
  lda [tcc__r1],y
  beq +
  bmi _far20
  brl loom_pvs_actor_fall
_far20:
  brl loom_pvs_actor_rise
+ lda.l loom_pvs_actor_grounded
  beq _far21
  brl loom_pvs_actor_fall
_far21:
  lda #0
  brl loom_pvs_actor_phase
loom_pvs_actor_fall:
  ; reach = next_y + box_y + box_h - 1 (+16 on the ground)
  ldy #16
  lda [tcc__r1],y
  ldy #10
  clc
  adc [tcc__r1],y
  ldy #14
  clc
  adc [tcc__r1],y
  dec a
  pha
  lda.l loom_pvs_actor_grounded
  tax
  pla
  cpx #0
  beq +
  clc
  adc #16
+ ldy #22
  sta [tcc__r1],y                       ; reach
  ; landed = loom_pvs_body_scan_floor(left, right, bottom, reach, sensor_x)
  ldy #32
  lda [tcc__r1],y
  pha
  ldy #22
  lda [tcc__r1],y
  pha
  ldy #30
  lda [tcc__r1],y
  pha
  ldy #26
  lda [tcc__r1],y
  pha
  ldy #24
  lda [tcc__r1],y
  pha
  jsl loom_pvs_body_scan_floor
  pla
  pla
  pla
  pla
  pla
  jsr loom_pvs_actor_pointers
  lda.b tcc__r0
  ldy #18
  sta [tcc__r1],y                       ; landed
  lda #1
  brl loom_pvs_actor_phase
loom_pvs_actor_rise:
  ; stopped = loom_pvs_body_scan_ceiling(left, right, top, next_y + box_y)
  ldy #16
  lda [tcc__r1],y
  ldy #10
  clc
  adc [tcc__r1],y
  pha
  ldy #28
  lda [tcc__r1],y
  pha
  ldy #26
  lda [tcc__r1],y
  pha
  ldy #24
  lda [tcc__r1],y
  pha
  jsl loom_pvs_body_scan_ceiling
  pla
  pla
  pla
  pla
  jsr loom_pvs_actor_pointers
  lda.b tcc__r0
  ldy #20
  sta [tcc__r1],y                       ; stopped
  lda #2
loom_pvs_actor_phase:
  ldy #43
  sep #$20
  sta [tcc__r1],y                       ; phase
  lda.l loom_pvs_actor_flags
  ldy #41
  sta [tcc__r1],y                       ; flags
  rep #$20
  rts

; ---------------------------------------------------------------------------
; The bound step: the record is filled from the pool's arrays here, the core
; runs, and -- for a room with no solid actor to stand on -- the slope under
; the sensor and the snap finish the step and the arrays are written back,
; so the C side makes one call per actor and copies nothing.

; void loom_pvs_actor_bind(loom_s16 *x, loom_s16 *y, loom_u8 *sub_x,
;     loom_u8 *sub_y, loom_s16 *vx, loom_s16 *vy, loom_s8 *intent_x,
;     loom_s8 *intent_y, loom_u8 *body_flags, loom_u8 *riding)
; Ten four-byte pointers: the i-th at 5 + 4 * i,s.
loom_pvs_actor_bind:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_actor_arr_x
  lda 7,s
  sta.l loom_pvs_actor_arr_x+2
  lda 9,s
  sta.l loom_pvs_actor_arr_y
  lda 11,s
  sta.l loom_pvs_actor_arr_y+2
  lda 13,s
  sta.l loom_pvs_actor_arr_sub_x
  lda 15,s
  sta.l loom_pvs_actor_arr_sub_x+2
  lda 17,s
  sta.l loom_pvs_actor_arr_sub_y
  lda 19,s
  sta.l loom_pvs_actor_arr_sub_y+2
  lda 21,s
  sta.l loom_pvs_actor_arr_vx
  lda 23,s
  sta.l loom_pvs_actor_arr_vx+2
  lda 25,s
  sta.l loom_pvs_actor_arr_vy
  lda 27,s
  sta.l loom_pvs_actor_arr_vy+2
  lda 29,s
  sta.l loom_pvs_actor_arr_intent_x
  lda 31,s
  sta.l loom_pvs_actor_arr_intent_x+2
  lda 33,s
  sta.l loom_pvs_actor_arr_intent_y
  lda 35,s
  sta.l loom_pvs_actor_arr_intent_y+2
  lda 37,s
  sta.l loom_pvs_actor_arr_body_flags
  lda 39,s
  sta.l loom_pvs_actor_arr_body_flags+2
  lda 41,s
  sta.l loom_pvs_actor_arr_riding
  lda 43,s
  sta.l loom_pvs_actor_arr_riding+2
  plp
  rtl


; ---------------------------------------------------------------------------
; The bound step, tight: the actor's state lives in the direct page for the
; whole step (816-tcc's scratch registers, which no C value survives a call
; in), the tile probes run inline on that state, and the pool's arrays are
; read at the start and written at the end. Nothing here is shared with the
; record step above, which stays for rooms with solid actors as floors.
;
; Direct page during the step:
;   r0/r0h  a pointer, then scratch     r1 x        r1h y
;   r2 vx   r2h vy                      r3 left     r3h right
;   r4 top  r4h bottom                  r5 sensor_x r5h next
;   r9/r9h the grid's cells pointer     r10/r10h the body pointer
;   f2 box_x  f2h box_y  f3 box_w  f3h box_h
; The rest (intent, grounded, flags, subpixels, reach, landed ...) sits in
; loom.pvs.actor.body.state and is read with long addressing.

; A <- word of the array whose pointer is at RAM address X, at idx2.
.MACRO LOOM_ACTOR_LOAD_WORD ARGS array, dest
  lda.l array
  sta.b tcc__r0
  lda.l array+2
  sta.b tcc__r0h
  lda.l loom_pvs_actor_idx2
  tay
  lda [tcc__r0],y
  sta.b dest
.ENDM
; dest (DP word) -> the array at idx2.
.MACRO LOOM_ACTOR_STORE_WORD ARGS array, src
  lda.l array
  sta.b tcc__r0
  lda.l array+2
  sta.b tcc__r0h
  lda.l loom_pvs_actor_idx2
  tay
  lda.b src
  sta [tcc__r0],y
.ENDM
; A (16-bit, 0..255) <- byte of the array at idx.
.MACRO LOOM_ACTOR_LOAD_BYTE ARGS array
  lda.l array
  sta.b tcc__r0
  lda.l array+2
  sta.b tcc__r0h
  lda.l loom_pvs_actor_idx
  tay
  lda [tcc__r0],y
  and #$00ff
.ENDM
; A's low byte -> the array at idx (A clobbered).
.MACRO LOOM_ACTOR_STORE_BYTE ARGS array
  sta.l loom_pvs_actor_tmp
  lda.l array
  sta.b tcc__r0
  lda.l array+2
  sta.b tcc__r0h
  lda.l loom_pvs_actor_idx
  tay
  lda.l loom_pvs_actor_tmp
  sep #$20
  sta [tcc__r0],y
  rep #$20
.ENDM

; ---------------------------------------------------------------------------
; Solid actors, for the assembly bodies (PERF-004): a ferry or a platform is
; a floor to land on and, for the player, a wall. The pool's queries are C
; (actor.c, reached through movement.c's shims so this unit links without
; the actor module); these call them the 816-tcc way with the direct page
; the bodies keep their state in saved around the call. They run only while
; loom_movement_solid_actors is nonzero.

.MACRO LOOM_SAVE_BODY_DP
  pei (tcc__r1)
  pei (tcc__r1h)
  pei (tcc__r2)
  pei (tcc__r2h)
  pei (tcc__r3)
  pei (tcc__r3h)
  pei (tcc__r4)
  pei (tcc__r4h)
  pei (tcc__r5)
  pei (tcc__r5h)
  pei (tcc__r9)
  pei (tcc__r9h)
  pei (tcc__r10)
  pei (tcc__r10h)
  pei (tcc__f2)
  pei (tcc__f2h)
  pei (tcc__f3)
  pei (tcc__f3h)
.ENDM
.MACRO LOOM_RESTORE_BODY_DP
  pla
  sta.b tcc__f3h
  pla
  sta.b tcc__f3
  pla
  sta.b tcc__f2h
  pla
  sta.b tcc__f2
  pla
  sta.b tcc__r10h
  pla
  sta.b tcc__r10
  pla
  sta.b tcc__r9h
  pla
  sta.b tcc__r9
  pla
  sta.b tcc__r5h
  pla
  sta.b tcc__r5
  pla
  sta.b tcc__r4h
  pla
  sta.b tcc__r4
  pla
  sta.b tcc__r3h
  pla
  sta.b tcc__r3
  pla
  sta.b tcc__r2h
  pla
  sta.b tcc__r2
  pla
  sta.b tcc__r1h
  pla
  sta.b tcc__r1
.ENDM

; A <- 1 when solid actors exist this tick.
loom_pvs_solids:
  lda.l loom_movement_solid_actors
  and #$00ff
  rts

; The highest solid actor top under columns left (r3) .. right (r3h), in
; rows feet + 1 (r4h + 1) .. reach: A <- its index or $ff, its feet row in
; loom_movement_probe_floor.
loom_pvs_probe_actor_floor:
  lda.b tcc__r3
  sta.l loom_pvs_sq_left
  lda.b tcc__r3h
  sta.l loom_pvs_sq_right
  lda.b tcc__r4h
  inc a
  sta.l loom_pvs_sq_top
  lda.l loom_pvs_actor_reach
  sta.l loom_pvs_sq_bottom
  jsr loom_pvs_solid_floor
  sta.l loom_pvs_probe_result
  lda.l loom_pvs_probe_result
  rts

; Does a solid actor cover the box left (r0h), top (r4), right (r0h + w -
; 1), bottom (loom_pvs_actor_wall_bottom)? A <- nonzero when it does.
loom_pvs_probe_actor_wall:
  lda.b tcc__r0h
  sta.l loom_pvs_sq_left
  clc
  adc.b tcc__f3
  dec a
  sta.l loom_pvs_sq_right
  lda.b tcc__r4
  sta.l loom_pvs_sq_top
  lda.l loom_pvs_actor_wall_bottom
  sta.l loom_pvs_sq_bottom
  jsr loom_pvs_solid_wall
  sta.l loom_pvs_probe_result
  lda.l loom_pvs_probe_result
  rts

; ---------------------------------------------------------------------------
; The pool's solid-actor queries (loom_actor_floor_below with no self, and
; loom_actor_blocks_box outside a C resolve), over the bound arrays with
; only tcc__r0/r0h touched, so the bodies' direct page needs no saving
; around them. The box is loom_pvs_sq_left/right/top/bottom (inclusive).

; void loom_pvs_solid_bind(loom_u8 *solid_slots 5,s; loom_u8 *visible 9,s)
loom_pvs_solid_bind:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_solid_slots
  lda 7,s
  sta.l loom_pvs_solid_slots+2
  lda 9,s
  sta.l loom_pvs_solid_visible
  lda 11,s
  sta.l loom_pvs_solid_visible+2
  plp
  rtl

; tcc__r0/r0h <- the four-byte pointer at the long address.
.MACRO LOOM_SQ_PTR ARGS pointer
  lda.l pointer
  sta.b tcc__r0
  lda.l pointer+2
  sta.b tcc__r0h
.ENDM

; The next visible solid actor from loom_pvs_sq_pos on: carry clear with
; its index in loom_pvs_sq_idx, its left in loom_pvs_sq_x, its top in
; loom_pvs_sq_y and its type in tcc__r0/r0h (width at 6, height at 8);
; carry set when there are no more. Advances loom_pvs_sq_pos past it.
loom_pvs_solid_next:
  rep #$30
  lda.l loom_movement_solid_actors
  and #$00ff
  cmp.l loom_pvs_sq_pos
  beq loom_pvs_solid_next_none
  bcs loom_pvs_solid_next_take
loom_pvs_solid_next_none:
  sec
  rts
loom_pvs_solid_next_take:
  LOOM_SQ_PTR loom_pvs_solid_slots
  lda.l loom_pvs_sq_pos
  tay
  inc a
  sta.l loom_pvs_sq_pos
  lda [tcc__r0],y
  and #$00ff
  sta.l loom_pvs_sq_idx
  ; a slot the scene never mapped, or a gated actor it has hidden
  tay
  LOOM_SQ_PTR loom_pvs_actor_arr_sprite_index
  lda [tcc__r0],y
  and #$00ff
  cmp #$00ff
  beq loom_pvs_solid_next
  tay
  LOOM_SQ_PTR loom_pvs_solid_visible
  lda [tcc__r0],y
  and #$00ff
  beq loom_pvs_solid_next
  ; x, y
  lda.l loom_pvs_sq_idx
  asl a
  tay
  LOOM_SQ_PTR loom_pvs_actor_arr_x
  lda [tcc__r0],y
  sta.l loom_pvs_sq_x
  LOOM_SQ_PTR loom_pvs_actor_arr_y
  lda [tcc__r0],y
  sta.l loom_pvs_sq_y
  ; the type: box_x at 2, box_y at 4
  tya
  asl a
  tay
  LOOM_SQ_PTR loom_pvs_actor_arr_type_ptr
  lda [tcc__r0],y
  sta.l loom_pvs_sq_type
  iny
  iny
  lda [tcc__r0],y
  sta.l loom_pvs_sq_type+2
  LOOM_SQ_PTR loom_pvs_sq_type
  ldy #2
  lda [tcc__r0],y
  clc
  adc.l loom_pvs_sq_x
  sta.l loom_pvs_sq_x
  ldy #4
  lda [tcc__r0],y
  clc
  adc.l loom_pvs_sq_y
  sta.l loom_pvs_sq_y
  clc
  rts

; A <- the index of the highest visible solid actor whose top lies in rows
; top..bottom and whose box meets columns left..right, else $ff; its top - 1
; in loom_movement_probe_floor. The first of equal tops wins, as in C.
loom_pvs_solid_floor:
  rep #$30
  lda #$00ff
  sta.l loom_pvs_sq_found
  lda #0
  sta.l loom_pvs_sq_pos
loom_pvs_solid_floor_loop:
  jsr loom_pvs_solid_next
  bcc +
  brl loom_pvs_solid_floor_done
+ ; actor_left > right: no
  lda.l loom_pvs_sq_right
  LOOM_SIGNED_CMP_L loom_pvs_sq_x
  bmi loom_pvs_solid_floor_loop
  ; actor_left + width - 1 < left: no
  ldy #6
  lda [tcc__r0],y
  clc
  adc.l loom_pvs_sq_x
  dec a
  LOOM_SIGNED_CMP_L loom_pvs_sq_left
  bmi loom_pvs_solid_floor_loop
  ; actor_top < top or actor_top > bottom: no
  lda.l loom_pvs_sq_y
  LOOM_SIGNED_CMP_L loom_pvs_sq_top
  bmi loom_pvs_solid_floor_loop
  lda.l loom_pvs_sq_bottom
  LOOM_SIGNED_CMP_L loom_pvs_sq_y
  bmi loom_pvs_solid_floor_loop
  ; the first found, or a higher top
  lda.l loom_pvs_sq_found
  cmp #$00ff
  beq +
  lda.l loom_pvs_sq_y
  LOOM_SIGNED_CMP_L loom_pvs_sq_best
  bpl loom_pvs_solid_floor_loop
+ lda.l loom_pvs_sq_idx
  sta.l loom_pvs_sq_found
  lda.l loom_pvs_sq_y
  sta.l loom_pvs_sq_best
  brl loom_pvs_solid_floor_loop
loom_pvs_solid_floor_done:
  lda.l loom_pvs_sq_found
  cmp #$00ff
  beq +
  lda.l loom_pvs_sq_best
  dec a
  sta.l loom_movement_probe_floor
  lda.l loom_pvs_sq_found
+ rts

; A <- 1 when a visible solid actor's box overlaps left..right, top..bottom.
loom_pvs_solid_wall:
  rep #$30
  lda #0
  sta.l loom_pvs_sq_pos
loom_pvs_solid_wall_loop:
  jsr loom_pvs_solid_next
  bcc +
  lda #0
  rts
+ ; left > actor_left + width - 1: no
  ldy #6
  lda [tcc__r0],y
  clc
  adc.l loom_pvs_sq_x
  dec a
  LOOM_SIGNED_CMP_L loom_pvs_sq_left
  bmi loom_pvs_solid_wall_loop
  ; right < actor_left: no
  lda.l loom_pvs_sq_right
  LOOM_SIGNED_CMP_L loom_pvs_sq_x
  bmi loom_pvs_solid_wall_loop
  ; top <= actor_top + height - 1 and bottom >= actor_top: yes
  ldy #8
  lda [tcc__r0],y
  clc
  adc.l loom_pvs_sq_y
  dec a
  LOOM_SIGNED_CMP_L loom_pvs_sq_top
  bmi loom_pvs_solid_wall_loop
  lda.l loom_pvs_sq_bottom
  LOOM_SIGNED_CMP_L loom_pvs_sq_y
  bmi loom_pvs_solid_wall_loop
  lda #1
  rts

; A platform under the body: when one lies above the tile floor (or there
; is none), it is the landing and the body rides it. riding[idx] is set
; either way by the caller's convention (the actor's array here).
loom_pvs_tight_platform:
  jsr loom_pvs_solids
  bne +
  rts
+ jsr loom_pvs_probe_actor_floor
  cmp #$00ff
  bne +
  rts
+ sta.b tcc__r0h                        ; the platform
  lda.l loom_pvs_actor_landed
  bmi +
  lda.l loom_movement_probe_floor
  LOOM_SIGNED_CMP_L loom_pvs_actor_landed
  bmi +
  rts                                   ; the tile floor is higher
+ lda.l loom_movement_probe_floor
  sta.l loom_pvs_actor_landed
  lda.b tcc__r0h
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_riding
  rts

; loom_u8 loom_pvs_actor_step_bound(loom_u16 index,           ;  5,s
;                                   const LoomActorType *type, ;  7,s /  9,s
;                                   const LoomPlatformerBody *body) ; 11,s / 13,s
loom_pvs_actor_step_bound:
  php
  rep #$30
  lda 5,s
  and #$00ff
  sta.l loom_pvs_actor_idx
  asl a
  sta.l loom_pvs_actor_idx2
  lda 11,s
  sta.b tcc__r10
  lda 13,s
  sta.b tcc__r10h
  lda 7,s
  sta.b tcc__r0
  lda 9,s
  sta.b tcc__r0h
  ldy #2
  lda [tcc__r0],y
  sta.b tcc__f2                         ; box_x
  ldy #4
  lda [tcc__r0],y
  sta.b tcc__f2h                        ; box_y
  ldy #6
  lda [tcc__r0],y
  sta.b tcc__f3                         ; box_w
  ldy #8
  lda [tcc__r0],y
  sta.b tcc__f3h                        ; box_h
  jsr loom_pvs_tight_core
  plp
  rtl

; The step itself, for the bound entry above and the actor pass below:
; idx/idx2 set, the body in r10, the box in f2/f3. r0 <- the body flags.
loom_pvs_tight_core:
  lda.l loom_movement_grid
  sta.b tcc__r9
  lda.l loom_movement_grid+2
  sta.b tcc__r9h
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_x tcc__r1
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_y tcc__r1h
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_vx tcc__r2
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_vy tcc__r2h
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_x
  bit #$0080
  beq +
  ora #$ff00
+ sta.l loom_pvs_actor_intent
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_y
  bit #$0080
  beq +
  ora #$ff00
+ sta.l loom_pvs_actor_intent_y
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_body_flags
  and #$0001
  sta.l loom_pvs_actor_grounded
  lda #0
  sta.l loom_pvs_actor_flags
  sta.l loom_pvs_actor_jumped
  ; the box
  lda.b tcc__r1
  clc
  adc.b tcc__f2
  sta.b tcc__r3                         ; left
  clc
  adc.b tcc__f3
  dec a
  sta.b tcc__r3h                        ; right
  lda.b tcc__r1h
  clc
  adc.b tcc__f2h
  sta.b tcc__r4                         ; top
  clc
  adc.b tcc__f3h
  dec a
  sta.b tcc__r4h                        ; bottom

  ; A ledge ahead is a wall to a walker that turns at them.
  lda.l loom_pvs_actor_intent
  bne +
  brl loom_pvs_tight_velocity_x
+ lda.l loom_pvs_actor_grounded
  bne +
  brl loom_pvs_tight_velocity_x
+ ldy #18
  lda [tcc__r10],y
  and #$0001
  bne +
  brl loom_pvs_tight_velocity_x
+ lda.l loom_pvs_actor_intent
  bmi +
  lda.b tcc__r3h
  bra ++
+ lda.b tcc__r3
++ clc
  adc.l loom_pvs_actor_intent
  tax                                   ; ahead
  lda.b tcc__r4h
  inc a                                 ; bottom + 1
  jsr loom_pvs_tight_cell
  beq _far201
  brl loom_pvs_tight_velocity_x
_far201:
  ; nothing under the step ahead: turn
  lda.l loom_pvs_actor_intent
  bmi +
  lda #4
  bra ++
+ lda #2
++ sta.l loom_pvs_actor_flags
  lda #0
  sta.l loom_pvs_actor_intent
  sta.b tcc__r2                         ; vx = 0

loom_pvs_tight_velocity_x:
  lda.l loom_pvs_actor_intent
  bne _far202
  brl loom_pvs_tight_friction
_far202:
  lda.l loom_pvs_actor_grounded
  beq +
  ldy #2
  lda [tcc__r10],y                      ; acceleration
  bra ++
+ ldy #6
  lda [tcc__r10],y                      ; air_control
++ sta.b tcc__r0
  lda.l loom_pvs_actor_intent
  bmi +
  lda.b tcc__r2
  clc
  adc.b tcc__r0
  bra ++
+ lda.b tcc__r2
  sec
  sbc.b tcc__r0
++ sta.b tcc__r2
  ; clamp to +-max_speed
  ldy #0
  lda [tcc__r10],y
  sta.b tcc__r0                         ; max_speed
  lda.b tcc__r2
  LOOM_SIGNED_CMP tcc__r0
  beq +
  bmi +
  lda.b tcc__r0
  sta.b tcc__r2
  brl loom_pvs_tight_velocity_y
+ lda.b tcc__r0
  eor #$ffff
  inc a
  sta.b tcc__r0                         ; -max_speed
  lda.b tcc__r2
  LOOM_SIGNED_CMP tcc__r0
  bmi _far203
  brl loom_pvs_tight_velocity_y
_far203:
  lda.b tcc__r0
  sta.b tcc__r2
  brl loom_pvs_tight_velocity_y
loom_pvs_tight_friction:
  lda.l loom_pvs_actor_grounded
  bne _far204
  brl loom_pvs_tight_velocity_y
_far204:
  ldy #4
  lda [tcc__r10],y
  sta.b tcc__r0                         ; friction
  lda.b tcc__r2
  bne _far205
  brl loom_pvs_tight_velocity_y
_far205:
  bmi +
  sec
  sbc.b tcc__r0
  bpl ++
  lda #0
  bra ++
+ clc
  adc.b tcc__r0
  bmi ++
  beq ++
  lda #0
++ sta.b tcc__r2

loom_pvs_tight_velocity_y:
  lda.l loom_pvs_actor_intent_y
  bpl _far206
  brl loom_pvs_tight_jump
_far206:
  beq _far207
  brl loom_pvs_tight_gravity
_far207:
  ; intent_y == 0: cut a rise faster than jump_cut when jump_cut < jump_speed
  ldy #14
  lda [tcc__r10],y                      ; jump_cut
  sta.b tcc__r0
  ldy #12
  lda [tcc__r10],y                      ; jump_speed
  cmp.b tcc__r0
  bne _far208
  brl loom_pvs_tight_gravity
_far208:
  bcs _far209
  brl loom_pvs_tight_gravity
_far209:
  lda.b tcc__r0
  eor #$ffff
  inc a
  sta.b tcc__r0                         ; -jump_cut
  lda.b tcc__r2h
  LOOM_SIGNED_CMP tcc__r0
  bmi _far210
  brl loom_pvs_tight_gravity
_far210:
  lda.b tcc__r0
  sta.b tcc__r2h
  brl loom_pvs_tight_gravity
loom_pvs_tight_jump:
  lda.l loom_pvs_actor_grounded
  bne _far211
  brl loom_pvs_tight_gravity
_far211:
  ldy #12
  lda [tcc__r10],y
  eor #$ffff
  inc a
  sta.b tcc__r2h                        ; -jump_speed
  lda #0
  sta.l loom_pvs_actor_grounded
  lda #1
  sta.l loom_pvs_actor_jumped
loom_pvs_tight_gravity:
  lda.l loom_pvs_actor_grounded
  beq _far212
  brl loom_pvs_tight_x
_far212:
  ldy #8
  lda.b tcc__r2h
  clc
  adc [tcc__r10],y                      ; + gravity
  sta.b tcc__r2h
  ldy #10
  lda [tcc__r10],y
  sta.b tcc__r0                         ; terminal_velocity
  lda.b tcc__r2h
  LOOM_SIGNED_CMP tcc__r0
  bne _far213
  brl loom_pvs_tight_x
_far213:
  bpl _far214
  brl loom_pvs_tight_x
_far214:
  lda.b tcc__r0
  sta.b tcc__r2h

loom_pvs_tight_x:
  ; X against solid cells, stepping the feet's half tile while grounded.
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_sub_x
  sta.l loom_pvs_actor_next_sub
  lda.b tcc__r2
  sta.b tcc__r0                         ; velocity
  lda.b tcc__r1
  jsr loom_pvs_tight_advance            ; A <- next, next_sub updated
  sta.b tcc__r5h
  cmp.b tcc__r1
  bne +
  brl loom_pvs_tight_x_done
+ sec
  sbc.b tcc__r1
  sta.l loom_pvs_actor_delta
  bmi +
  lda.b tcc__r3h
  bra ++
+ lda.b tcc__r3
++ sta.l loom_pvs_actor_edge
  lda.b tcc__r4h
  sta.l loom_pvs_actor_wall_bottom
  lda.l loom_pvs_actor_grounded
  beq +
  lda.b tcc__r4h
  sec
  sbc #8
  LOOM_SIGNED_CMP tcc__r4
  bpl ++
  lda.b tcc__r4                         ; wall_bottom < top: top
  bra +++
++ lda.b tcc__r4h
  sec
  sbc #8
+++ sta.l loom_pvs_actor_wall_bottom
+ jsr loom_pvs_tight_probe_x            ; r0 <- probe or $7fff
  lda.b tcc__r0
  cmp #$7fff
  bne _far215
  brl loom_pvs_tight_x_store
_far215:
  ; stop = x + (probe - edge) - sign; a wall; vx = 0; sub = 0
  sec
  sbc.l loom_pvs_actor_edge
  clc
  adc.b tcc__r1
  sta.b tcc__r5h
  lda.l loom_pvs_actor_delta
  bmi +
  dec.b tcc__r5h
  lda.l loom_pvs_actor_flags
  ora #4
  sta.l loom_pvs_actor_flags
  bra ++
+ inc.b tcc__r5h
  lda.l loom_pvs_actor_flags
  ora #2
  sta.l loom_pvs_actor_flags
++ lda #0
  sta.b tcc__r2                         ; vx = 0
  sta.l loom_pvs_actor_next_sub
loom_pvs_tight_x_store:
  lda.b tcc__r5h
  sta.b tcc__r1                         ; x = stop
  clc
  adc.b tcc__f2
  sta.b tcc__r3                         ; left
  clc
  adc.b tcc__f3
  dec a
  sta.b tcc__r3h                        ; right
loom_pvs_tight_x_done:
  lda.l loom_pvs_actor_next_sub
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_sub_x
  lda.b tcc__f3
  lsr a
  clc
  adc.b tcc__r3
  sta.b tcc__r5                         ; sensor_x = left + box_w / 2

  ; Y: the move, then the floor scan or the ceiling scan.
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_sub_y
  sta.l loom_pvs_actor_next_sub
  lda.b tcc__r2h
  sta.b tcc__r0
  lda.b tcc__r1h
  jsr loom_pvs_tight_advance
  sta.b tcc__r5h                        ; next_y
  lda.b tcc__r2h
  beq +
  bmi _far216
  brl loom_pvs_tight_fall
_far216:
  brl loom_pvs_tight_rise
+ lda.l loom_pvs_actor_grounded
  beq _far217
  brl loom_pvs_tight_fall
_far217:
  brl loom_pvs_tight_free
loom_pvs_tight_fall:
  ; reach = next_y + box_y + box_h - 1 (+16 on the ground)
  lda.b tcc__r5h
  clc
  adc.b tcc__f2h
  clc
  adc.b tcc__f3h
  dec a
  sta.l loom_pvs_actor_reach
  lda.l loom_pvs_actor_grounded
  beq +
  lda.l loom_pvs_actor_reach
  clc
  adc #16
  sta.l loom_pvs_actor_reach
+ jsr loom_pvs_tight_scan_floor         ; landed (RAM)
  jsr loom_pvs_tight_riding_none
  jsr loom_pvs_tight_platform           ; a solid actor above the tiles
  ; the slope under the sensor at the feet: floor < landed (or no landing) wins
  ldx.b tcc__r5
  lda.b tcc__r4h
  jsr loom_pvs_tight_cell
  cmp #3
  beq +
  cmp #4
  beq ++
  brl loom_pvs_tight_snap
+ lda.b tcc__r5
  and #$000f
  eor #$ffff
  sec
  adc #15                               ; 15 - (sensor & 15)
  bra +++
++ lda.b tcc__r5
  and #$000f
+++ sta.b tcc__r0
  lda.b tcc__r4h
  and #$fff0
  clc
  adc.b tcc__r0                         ; floor
  sta.b tcc__r0
  lda.l loom_pvs_actor_landed
  bmi ++++
  lda.b tcc__r0
  LOOM_SIGNED_CMP_L loom_pvs_actor_landed
  bmi _far218
  brl loom_pvs_tight_snap
_far218:
++++ lda.b tcc__r0
  sta.l loom_pvs_actor_landed
  jsr loom_pvs_tight_riding_none        ; a slope is ground, not a rider's deck
loom_pvs_tight_snap:
  lda.l loom_pvs_actor_landed
  bpl _far219
  brl loom_pvs_tight_fell
_far219:
  LOOM_SIGNED_CMP_L loom_pvs_actor_reach
  beq +
  bmi _far220             ; landed > reach
  brl loom_pvs_tight_fell
_far220:
+ lda.l loom_pvs_actor_landed
  sec
  sbc.b tcc__f3h
  inc a
  sec
  sbc.b tcc__f2h
  sta.b tcc__r1h                        ; y
  lda #0
  sta.l loom_pvs_actor_next_sub
  sta.b tcc__r2h                        ; vy = 0
  lda.l loom_pvs_actor_flags
  ora #1
  sta.l loom_pvs_actor_flags
  lda.l loom_pvs_actor_grounded
  beq _far221
  brl loom_pvs_tight_write
_far221:
  lda.l loom_pvs_actor_flags
  ora #8
  sta.l loom_pvs_actor_flags
  brl loom_pvs_tight_write
loom_pvs_tight_rise:
  jsr loom_pvs_tight_riding_none
  lda.b tcc__r5h
  clc
  adc.b tcc__f2h
  sta.l loom_pvs_actor_reach            ; next_y + box_y
  jsr loom_pvs_tight_scan_ceiling       ; stopped (RAM)
  lda.l loom_pvs_actor_stopped
  bpl _far222
  brl loom_pvs_tight_free_y
_far222:
  sec
  sbc.b tcc__f2h
  sta.b tcc__r1h                        ; y = stopped - box_y
  lda #0
  sta.l loom_pvs_actor_next_sub
  sta.b tcc__r2h
  brl loom_pvs_tight_write
loom_pvs_tight_fell:
  jsr loom_pvs_tight_riding_none        ; nothing under the feet
loom_pvs_tight_free:
loom_pvs_tight_free_y:
  lda.b tcc__r5h
  sta.b tcc__r1h                        ; y = next
loom_pvs_tight_write:
  lda.l loom_pvs_actor_next_sub
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_sub_y
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_x tcc__r1
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_y tcc__r1h
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_vx tcc__r2
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_vy tcc__r2h
  lda.l loom_pvs_actor_jumped
  beq +
  jsr loom_pvs_tight_riding_none
+ lda.l loom_pvs_actor_flags
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_body_flags
  lda.l loom_pvs_actor_flags
  and #$00ff
  sta.b tcc__r0
  rts

; riding[idx] = $ff
loom_pvs_tight_riding_none:
  lda #$00ff
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_riding
  rts

; The cell at (X, A), solid outside the room; A <- cell. Keeps r0-r5, r9,
; r10; clobbers X, Y and r0h.
loom_pvs_tight_cell:
  bpl _far301
  brl loom_pvs_tight_cell_solid
_far301:
  cmp.l loom_movement_grid+6
  bcc _far302
  brl loom_pvs_tight_cell_solid
_far302:
  sta.b tcc__r0h
  txa
  bpl _far303
  brl loom_pvs_tight_cell_solid
_far303:
  cmp.l loom_movement_grid+4
  bcc _far304
  brl loom_pvs_tight_cell_solid
_far304:
  lsr a
  lsr a
  lsr a
  lsr a
  pha
  lda.b tcc__r0h
  lsr a
  lsr a
  lsr a
  and #$fffe
  tax
  lda.l loom_movement_grid+8,x
  sta.b tcc__r0h
  pla
  clc
  adc.b tcc__r0h
  tay
  sep #$20
  lda [tcc__r9],y
  rep #$20
  and #$00ff
  rts
loom_pvs_tight_cell_solid:
  lda #1
  rts

; A (whole) advances by the velocity in r0 with loom_pvs_actor_next_sub:
; LOOM_MOVEMENT_ADVANCE. A <- new whole; next_sub updated. Clobbers r0h.
loom_pvs_tight_advance:
  sta.b tcc__r0h                        ; whole
  lda.b tcc__r0
  bne _far305
  brl loom_pvs_tight_advance_done
_far305:
  bpl _far306
  brl loom_pvs_tight_advance_back
_far306:
  and #$00ff
  clc
  adc.l loom_pvs_actor_next_sub         ; c
  pha
  and #$00ff
  sta.l loom_pvs_actor_next_sub
  pla
  xba
  and #$00ff                            ; c >> 8
  clc
  adc.b tcc__r0h
  sta.b tcc__r0h
  lda.b tcc__r0
  xba
  and #$00ff                            ; v >> 8
  clc
  adc.b tcc__r0h
  sta.b tcc__r0h
  brl loom_pvs_tight_advance_done
loom_pvs_tight_advance_back:
  eor #$ffff
  inc a                                 ; m
  pha
  and #$00ff                            ; f
  sta.b tcc__r0
  lda.l loom_pvs_actor_next_sub
  cmp.b tcc__r0
  bcs +
  dec.b tcc__r0h                        ; borrow
+ sec
  sbc.b tcc__r0
  and #$00ff
  sta.l loom_pvs_actor_next_sub
  pla
  xba
  and #$00ff                            ; m >> 8
  sta.b tcc__r0
  lda.b tcc__r0h
  sec
  sbc.b tcc__r0
  sta.b tcc__r0h
loom_pvs_tight_advance_done:
  lda.b tcc__r0h
  rts

; The wall probe: columns from edge + sign through edge + delta (whole
; tiles after the first), rows top (r4) through wall_bottom. r0 <- the
; blocking column or $7fff. Clobbers r0h, X, Y; the column in
; loom_pvs_actor_probe, the row in loom_pvs_actor_row.
loom_pvs_tight_probe_x:
  lda.l loom_pvs_actor_edge
  clc
  adc.l loom_pvs_actor_delta
  sta.l loom_pvs_actor_bound
  lda.l loom_pvs_actor_delta
  bpl _far307
  brl loom_pvs_tight_probe_left
_far307:
  lda.l loom_pvs_actor_edge
  inc a
  sta.l loom_pvs_actor_probe
loom_pvs_tight_probe_right_loop:
  lda.l loom_pvs_actor_probe
  LOOM_SIGNED_CMP_L loom_pvs_actor_bound
  beq +
  bmi _far308
  brl loom_pvs_tight_probe_none
_far308:
+ jsr loom_pvs_tight_probe_column
  bcc _far309
  brl loom_pvs_tight_probe_hit
_far309:
  lda.l loom_pvs_actor_probe
  and #$000f
  cmp #$000f
  beq +
  lda.l loom_pvs_actor_probe
  ora #$000f
  sta.l loom_pvs_actor_probe
+ lda.l loom_pvs_actor_probe
  inc a
  sta.l loom_pvs_actor_probe
  brl loom_pvs_tight_probe_right_loop
loom_pvs_tight_probe_left:
  lda.l loom_pvs_actor_edge
  dec a
  sta.l loom_pvs_actor_probe
loom_pvs_tight_probe_left_loop:
  lda.l loom_pvs_actor_probe
  LOOM_SIGNED_CMP_L loom_pvs_actor_bound
  bpl _far310
  brl loom_pvs_tight_probe_none
_far310:
  jsr loom_pvs_tight_probe_column
  bcc _far311
  brl loom_pvs_tight_probe_hit
_far311:
  lda.l loom_pvs_actor_probe
  and #$000f
  beq +
  lda.l loom_pvs_actor_probe
  and #$fff0
  sta.l loom_pvs_actor_probe
+ lda.l loom_pvs_actor_probe
  dec a
  sta.l loom_pvs_actor_probe
  brl loom_pvs_tight_probe_left_loop
loom_pvs_tight_probe_none:
  lda #$7fff
  sta.b tcc__r0
  rts
loom_pvs_tight_probe_hit:
  lda.l loom_pvs_actor_probe
  sta.b tcc__r0
  rts
; Column loom_pvs_actor_probe, rows top through wall_bottom: carry when solid.
loom_pvs_tight_probe_column:
  lda.b tcc__r4
  sta.l loom_pvs_actor_row
- lda.l loom_pvs_actor_row
  LOOM_SIGNED_CMP_L loom_pvs_actor_wall_bottom
  beq +
  bmi _far312
  brl loom_pvs_tight_probe_column_clear
_far312:
+ lda.l loom_pvs_actor_probe
  tax
  lda.l loom_pvs_actor_row
  jsr loom_pvs_tight_cell
  cmp #1
  bne _far313
  brl loom_pvs_tight_probe_column_solid
_far313:
  lda.l loom_pvs_actor_row
  ora #$000f
  inc a
  sta.l loom_pvs_actor_row
  bra -
loom_pvs_tight_probe_column_solid:
  sec
  rts
loom_pvs_tight_probe_column_clear:
  clc
  rts

; The floor scan: rows from (bottom + 1) & ~15 to reach; a slope under the
; sensor decides its row, else a solid cell across left..right or a one-way
; whose row starts below the feet. loom_pvs_actor_landed <- floor or -1.
loom_pvs_tight_scan_floor:
  lda #$ffff
  sta.l loom_pvs_actor_landed
  lda.b tcc__r4h
  inc a
  and #$fff0
  sta.l loom_pvs_actor_row
loom_pvs_tight_floor_row:
  lda.l loom_pvs_actor_row
  LOOM_SIGNED_CMP_L loom_pvs_actor_reach
  beq +
  bmi _far314         ; row_top > reach
  brl loom_pvs_tight_floor_done
_far314:
+ ldx.b tcc__r5
  lda.l loom_pvs_actor_row
  jsr loom_pvs_tight_cell
  cmp #3
  bne _far315
  brl loom_pvs_tight_floor_slope_right
_far315:
  cmp #4
  bne _far316
  brl loom_pvs_tight_floor_slope_left
_far316:
  lda.b tcc__r3
  sta.l loom_pvs_actor_probe            ; the column
loom_pvs_tight_floor_column:
  lda.l loom_pvs_actor_probe
  LOOM_SIGNED_CMP tcc__r3h
  beq +
  bmi _far317         ; x > right
  brl loom_pvs_tight_floor_next
_far317:
+ lda.l loom_pvs_actor_probe
  tax
  lda.l loom_pvs_actor_row
  jsr loom_pvs_tight_cell
  cmp #1
  bne _far318
  brl loom_pvs_tight_floor_lip
_far318:
  cmp #2
  beq _far319
  brl loom_pvs_tight_floor_column_next
_far319:
  lda.b tcc__r4h
  LOOM_SIGNED_CMP_L loom_pvs_actor_row
  bpl _far320          ; feet above the row
  brl loom_pvs_tight_floor_lip
_far320:
loom_pvs_tight_floor_column_next:
  lda.l loom_pvs_actor_probe
  ora #$000f
  inc a
  sta.l loom_pvs_actor_probe
  brl loom_pvs_tight_floor_column
loom_pvs_tight_floor_lip:
  lda.l loom_pvs_actor_row
  dec a
  sta.l loom_pvs_actor_landed
  rts
loom_pvs_tight_floor_slope_right:
  lda.b tcc__r5
  and #$000f
  eor #$ffff
  sec
  adc.l loom_pvs_actor_row
  clc
  adc #15
  brl loom_pvs_tight_floor_slope
loom_pvs_tight_floor_slope_left:
  lda.b tcc__r5
  and #$000f
  clc
  adc.l loom_pvs_actor_row
loom_pvs_tight_floor_slope:
  sta.b tcc__r0                         ; floor
  lda.b tcc__r4h
  sec
  sbc #16
  sta.b tcc__r0h                        ; feet - 16
  lda.b tcc__r0
  LOOM_SIGNED_CMP tcc__r0h
  bpl _far321
  brl loom_pvs_tight_floor_next
_far321:
  lda.b tcc__r0
  LOOM_SIGNED_CMP_L loom_pvs_actor_reach
  beq +
  bmi _far322
  brl loom_pvs_tight_floor_next
_far322:
+ lda.b tcc__r0
  sta.l loom_pvs_actor_landed
  rts
loom_pvs_tight_floor_next:
  lda.l loom_pvs_actor_row
  clc
  adc #16
  sta.l loom_pvs_actor_row
  brl loom_pvs_tight_floor_row
loom_pvs_tight_floor_done:
  rts

; The ceiling scan: rows from (top - 1) & ~15 down while row + 15 >= reach;
; the first solid across left..right stops at row + 16. stopped <- or -1.
loom_pvs_tight_scan_ceiling:
  lda #$ffff
  sta.l loom_pvs_actor_stopped
  lda.b tcc__r4
  dec a
  and #$fff0
  sta.l loom_pvs_actor_row
loom_pvs_tight_ceiling_row:
  lda.l loom_pvs_actor_row
  clc
  adc #15
  LOOM_SIGNED_CMP_L loom_pvs_actor_reach
  bpl _far323
  brl loom_pvs_tight_ceiling_done
_far323:
  lda.b tcc__r3
  sta.l loom_pvs_actor_probe
loom_pvs_tight_ceiling_column:
  lda.l loom_pvs_actor_probe
  LOOM_SIGNED_CMP tcc__r3h
  beq +
  bmi _far324
  brl loom_pvs_tight_ceiling_next
_far324:
+ lda.l loom_pvs_actor_probe
  tax
  lda.l loom_pvs_actor_row
  jsr loom_pvs_tight_cell
  cmp #1
  bne _far325
  brl loom_pvs_tight_ceiling_hit
_far325:
  lda.l loom_pvs_actor_probe
  ora #$000f
  inc a
  sta.l loom_pvs_actor_probe
  brl loom_pvs_tight_ceiling_column
loom_pvs_tight_ceiling_next:
  lda.l loom_pvs_actor_row
  sec
  sbc #16
  sta.l loom_pvs_actor_row
  brl loom_pvs_tight_ceiling_row
loom_pvs_tight_ceiling_hit:
  lda.l loom_pvs_actor_row
  clc
  adc #16
  sta.l loom_pvs_actor_stopped
loom_pvs_tight_ceiling_done:
  rts

; ---------------------------------------------------------------------------
; The player's platformer tick (PERF-004): loom_movement_platformer_tick with
; the body's state held in the direct page for the tick and the tile probes
; above run inline. Rooms with solid actors keep the C tick (a platform is a
; wall and a floor there); the wrapper in movement.c picks.
;
; The direct page during the tick, as in the actor step: r1 x, r1h y, r2 vx,
; r2h vy, r3 left, r3h right, r4 top, r4h bottom, r5 sensor_x, r5h next,
; r9 the cells, r10 the body, f2 collider_x, f2h collider_y, f3 the width,
; f3h the height; r0/r0h scratch.
;
; Field offsets follow 816-tcc's layout (words aligned to two bytes).
; LoomMovementState: player_x 0, player_y 2, subpixel_x 4, subpixel_y 5,
; blocked_count 6, last_collision 8, moving 10, facing_x 11, velocity_x 14,
; velocity_y 16, on_ground 18, coyote_left 19, buffer_left 20, jumping 21,
; wall 22, riding 23, landed 24, dash_meter 26, scene 36 -- 40 bytes, which
; movement.c asserts as LOOM_MOVEMENT_STATE_BYTES. LoomMovementScene:
; collider_x 8, collider_y 10, collider_width 12, collider_height 14,
; platformer 28. LoomPlatformerBody: max_speed 0, acceleration 2, friction
; 4, air_control 6, gravity 8, terminal_velocity 10, jump_speed 12, jump_cut
; 14, coyote_ticks 16, buffer_ticks 17, flags 18, run_speed 20, dash_speed
; 22, skid 24, jump_speed_fast 26, gravity_hold 28, dash_ticks 30.
;
; void loom_pvs_player_tick(loom_u16 held,      ; 5,s
;                           loom_u16 pressed)   ; 7,s
loom_pvs_player_tick:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_player_held
  lda 7,s
  sta.l loom_pvs_player_pressed
  lda.l loom_movement_state+36
  sta.b tcc__r0
  lda.l loom_movement_state+38
  sta.b tcc__r0h                        ; the scene
  ldy #8
  lda [tcc__r0],y
  sta.b tcc__f2                         ; collider_x
  ldy #10
  lda [tcc__r0],y
  sta.b tcc__f2h                        ; collider_y
  ldy #12
  lda [tcc__r0],y
  sta.b tcc__f3                         ; collider_width
  ldy #14
  lda [tcc__r0],y
  sta.b tcc__f3h                        ; collider_height
  ldy #28
  lda [tcc__r0],y
  sta.b tcc__r10
  ldy #30
  lda [tcc__r0],y
  sta.b tcc__r10h                       ; the body
  lda.l loom_movement_grid
  sta.b tcc__r9
  lda.l loom_movement_grid+2
  sta.b tcc__r9h
  lda.l loom_movement_state+0
  sta.b tcc__r1                         ; x
  lda.l loom_movement_state+2
  sta.b tcc__r1h                        ; y
  lda.l loom_movement_state+14
  sta.b tcc__r2                         ; vx
  lda.l loom_movement_state+16
  sta.b tcc__r2h                        ; vy
  lda.l loom_movement_state+18
  and #$00ff
  sta.l loom_pvs_actor_grounded
  ; intent = right - left
  ldx #0
  lda.l loom_pvs_player_held
  bit #$0100
  beq +
  inx
+ bit #$0200
  beq +
  dex
+ txa
  sta.l loom_pvs_actor_intent

  ; The cap this tick: max_speed; run_speed with Y or X held; dash_speed
  ; once the meter has counted dash_ticks at the run on the ground. The
  ; meter holds in the air while the run button stays down.
  lda #0
  sta.l loom_pvs_player_speed
  sta.l loom_pvs_player_running
  sta.l loom_pvs_player_extended
  ldy #0
  lda [tcc__r10],y
  sta.l loom_pvs_player_cap
  ldy #18
  lda [tcc__r10],y
  and #$0002                            ; LOOM_PLATFORMER_FLAG_RUN
  bne +
  brl loom_pvs_player_h
+ sta.l loom_pvs_player_extended
  lda.b tcc__r2
  bpl +
  eor #$ffff
  inc a
+ sta.l loom_pvs_player_speed
  ldy #20
  lda [tcc__r10],y                      ; run_speed
  beq +
  lda.l loom_pvs_player_held
  and #$4040                            ; Y or X
  beq +
  sta.l loom_pvs_player_running
  lda [tcc__r10],y
  sta.l loom_pvs_player_cap
+ ldy #22
  lda [tcc__r10],y                      ; dash_speed
  bne +
  brl loom_pvs_player_h
+ ldy #30
  lda [tcc__r10],y
  and #$00ff                            ; dash_ticks
  bne +
  brl loom_pvs_player_h
+ sta.b tcc__r0
  lda.l loom_pvs_player_running
  beq loom_pvs_player_dash_drain
  lda.l loom_pvs_actor_grounded
  beq loom_pvs_player_dash_check        ; running in the air: the meter holds
  ldy #20
  lda [tcc__r10],y
  ldy #2
  sec
  sbc [tcc__r10],y                      ; run_speed - acceleration
  sta.b tcc__r0h
  lda.l loom_pvs_player_speed
  LOOM_SIGNED_CMP tcc__r0h
  bmi loom_pvs_player_dash_drain        ; under the run: drain
  lda.l loom_movement_state+26
  and #$00ff
  cmp.b tcc__r0
  bcs loom_pvs_player_dash_check
  inc a
  sep #$20
  sta.l loom_movement_state+26
  rep #$20
  bra loom_pvs_player_dash_check
loom_pvs_player_dash_drain:
  lda.l loom_movement_state+26
  and #$00ff
  beq loom_pvs_player_dash_check
  dec a
  sep #$20
  sta.l loom_movement_state+26
  rep #$20
loom_pvs_player_dash_check:
  lda.l loom_movement_state+26
  and #$00ff
  cmp.b tcc__r0
  bcc loom_pvs_player_h
  ldy #22
  lda [tcc__r10],y
  sta.l loom_pvs_player_cap

  ; Horizontal: toward the intent by the air control, the skid against the
  ; run on the ground, or the acceleration; into the cap, or eased back
  ; toward a cap that dropped; else friction on the ground.
loom_pvs_player_h:
  lda.l loom_pvs_actor_intent
  bne +
  brl loom_pvs_player_h_friction
+ lda.l loom_pvs_actor_grounded
  bne +
  ldy #6                                ; air_control
  bra loom_pvs_player_h_gain
+ lda.l loom_pvs_player_extended
  beq loom_pvs_player_h_accel
  ldy #24
  lda [tcc__r10],y                      ; skid
  beq loom_pvs_player_h_accel
  lda.l loom_pvs_actor_intent
  bmi +
  lda.b tcc__r2
  bmi loom_pvs_player_h_skid            ; right against a leftward run
  bra loom_pvs_player_h_accel
+ lda.b tcc__r2
  beq loom_pvs_player_h_accel
  bmi loom_pvs_player_h_accel
loom_pvs_player_h_skid:
  ldy #24
  bra loom_pvs_player_h_gain
loom_pvs_player_h_accel:
  ldy #2
loom_pvs_player_h_gain:
  lda [tcc__r10],y
  sta.b tcc__r0                         ; the gain
  lda.l loom_pvs_actor_intent
  bmi +
  lda.b tcc__r2
  clc
  adc.b tcc__r0
  bra ++
+ lda.b tcc__r2
  sec
  sbc.b tcc__r0
++ sta.b tcc__r2
  lda.l loom_pvs_player_extended
  beq loom_pvs_player_h_clamp
  lda.l loom_pvs_player_speed
  LOOM_SIGNED_CMP_L loom_pvs_player_cap
  beq loom_pvs_player_h_clamp
  bmi loom_pvs_player_h_clamp           ; speed <= cap: into the cap
  lda.b tcc__r2
  LOOM_SIGNED_CMP_L loom_pvs_player_cap
  beq loom_pvs_player_h_facing
  bmi loom_pvs_player_h_over_neg
  jsr loom_pvs_player_ease              ; over the cap: ease back
  sta.b tcc__r2
  bra loom_pvs_player_h_facing
loom_pvs_player_h_over_neg:
  lda.l loom_pvs_player_cap
  eor #$ffff
  inc a
  sta.b tcc__r0h                        ; -cap
  lda.b tcc__r2
  LOOM_SIGNED_CMP tcc__r0h
  bpl loom_pvs_player_h_facing
  jsr loom_pvs_player_ease
  eor #$ffff
  inc a
  sta.b tcc__r2
  bra loom_pvs_player_h_facing
loom_pvs_player_h_clamp:
  lda.b tcc__r2
  LOOM_SIGNED_CMP_L loom_pvs_player_cap
  beq loom_pvs_player_h_facing
  bmi +
  lda.l loom_pvs_player_cap
  sta.b tcc__r2
  bra loom_pvs_player_h_facing
+ lda.l loom_pvs_player_cap
  eor #$ffff
  inc a
  sta.b tcc__r0h
  lda.b tcc__r2
  LOOM_SIGNED_CMP tcc__r0h
  bpl loom_pvs_player_h_facing
  lda.b tcc__r0h
  sta.b tcc__r2
loom_pvs_player_h_facing:
  lda.l loom_pvs_actor_intent
  sep #$20
  sta.l loom_movement_state+11          ; facing_x
  rep #$20
  brl loom_pvs_player_jump
loom_pvs_player_h_friction:
  lda.l loom_pvs_actor_grounded
  bne +
  brl loom_pvs_player_jump
+ ldy #4
  lda [tcc__r10],y
  sta.b tcc__r0                         ; friction
  lda.b tcc__r2
  bne +
  brl loom_pvs_player_jump
+ bmi +
  sec
  sbc.b tcc__r0
  bpl ++
  lda #0
  bra ++
+ clc
  adc.b tcc__r0
  bmi ++
  beq ++
  lda #0
++ sta.b tcc__r2

  ; Jump: a press (or one buffered within buffer_ticks) on the ground or
  ; within coyote_ticks of leaving it; the speed grows with the run.
loom_pvs_player_jump:
  lda #0
  sta.l loom_pvs_player_jumped
  lda.l loom_pvs_player_pressed
  bit #$8000                            ; B
  beq +
  ldy #17
  lda [tcc__r10],y
  and #$00ff
  inc a                                 ; buffer_ticks + 1
  sep #$20
  sta.l loom_movement_state+20
  rep #$20
+ lda.l loom_movement_state+20
  and #$00ff                            ; buffer_left
  bne +
  brl loom_pvs_player_jump_counters
+ lda.l loom_pvs_actor_grounded
  bne +
  lda.l loom_movement_state+19
  and #$00ff                            ; coyote_left
  bne +
  brl loom_pvs_player_jump_counters
+ ldy #12
  lda [tcc__r10],y
  sta.l loom_pvs_actor_tmp              ; jump_speed
  lda.l loom_pvs_player_extended
  bne +
  brl loom_pvs_player_jump_go
+ ldy #26
  lda [tcc__r10],y                      ; jump_speed_fast
  bne +
  brl loom_pvs_player_jump_go
+  ldy #22
  lda [tcc__r10],y
  bne +
  ldy #20
  lda [tcc__r10],y
  bne +
  ldy #0
  lda [tcc__r10],y
+ sta.l loom_pvs_player_climb           ; top: the fastest cap the body has
  lda.l loom_pvs_player_speed
  LOOM_SIGNED_CMP_L loom_pvs_player_climb
  beq +
  bmi +
  lda.l loom_pvs_player_climb
  sta.l loom_pvs_player_speed           ; speed capped at top
+ ldy #26
  lda [tcc__r10],y
  cmp.l loom_pvs_actor_tmp
  bcc loom_pvs_player_jump_loss
  sec
  sbc.l loom_pvs_actor_tmp              ; jump_speed_fast - jump_speed
  jsr loom_pvs_player_jump_scale
  asl a
  asl a
  asl a
  asl a
  clc
  adc.l loom_pvs_actor_tmp
  sta.l loom_pvs_actor_tmp
  bra loom_pvs_player_jump_go
loom_pvs_player_jump_loss:
  lda.l loom_pvs_actor_tmp
  sec
  sbc [tcc__r10],y                      ; jump_speed - jump_speed_fast
  jsr loom_pvs_player_jump_scale
  asl a
  asl a
  asl a
  asl a
  sta.b tcc__r0
  lda.l loom_pvs_actor_tmp
  sec
  sbc.b tcc__r0
  sta.l loom_pvs_actor_tmp
loom_pvs_player_jump_go:
  lda.l loom_pvs_actor_tmp
  eor #$ffff
  inc a
  sta.b tcc__r2h                        ; vy = -jump_speed
  lda #0
  sta.l loom_pvs_actor_grounded
  sep #$20
  sta.l loom_movement_state+18          ; on_ground
  sta.l loom_movement_state+19          ; coyote_left
  sta.l loom_movement_state+20          ; buffer_left
  lda #1
  sta.l loom_movement_state+21          ; jumping
  rep #$20
  lda #1
  sta.l loom_pvs_player_jumped
loom_pvs_player_jump_counters:
  lda.l loom_movement_state+20
  and #$00ff
  beq +
  dec a
  sep #$20
  sta.l loom_movement_state+20
  rep #$20
+ lda.l loom_movement_state+19
  and #$00ff
  beq +
  dec a
  sep #$20
  sta.l loom_movement_state+19
  rep #$20
+ ; Releasing early cuts the rise to jump_cut (none when jump_cut is the
  ; jump_speed); the rise over, the jump is done.
  lda.l loom_movement_state+21
  and #$00ff                            ; jumping
  bne +
  brl loom_pvs_player_gravity
+ lda.l loom_pvs_player_held
  bit #$8000
  bne loom_pvs_player_cut_done
  lda.l loom_pvs_player_jumped
  bne loom_pvs_player_cut_done
  ldy #14
  lda [tcc__r10],y                      ; jump_cut
  sta.b tcc__r0
  ldy #12
  cmp [tcc__r10],y
  bcs loom_pvs_player_cut_done          ; jump_cut >= jump_speed: no cut
  lda.b tcc__r0
  eor #$ffff
  inc a
  sta.b tcc__r0                         ; -jump_cut
  lda.b tcc__r2h
  LOOM_SIGNED_CMP tcc__r0
  bpl loom_pvs_player_cut_done
  lda.b tcc__r0
  sta.b tcc__r2h
loom_pvs_player_cut_done:
  lda.b tcc__r2h
  bmi loom_pvs_player_gravity
  sep #$20
  lda #0
  sta.l loom_movement_state+21          ; jumping = 0
  rep #$20
loom_pvs_player_gravity:
  lda.l loom_pvs_actor_grounded
  beq +
  brl loom_pvs_player_x
+ ldy #8                                ; gravity
  lda.l loom_pvs_player_extended
  beq +
  lda.l loom_pvs_player_held
  bit #$8000
  beq +
  ldy #28
  lda [tcc__r10],y                      ; gravity_hold while B is held
  bne +
  ldy #8
+ lda.b tcc__r2h
  clc
  adc [tcc__r10],y
  sta.b tcc__r2h
  ldy #10
  lda [tcc__r10],y
  sta.b tcc__r0                         ; terminal_velocity
  lda.b tcc__r2h
  LOOM_SIGNED_CMP tcc__r0
  beq loom_pvs_player_x
  bmi loom_pvs_player_x
  lda.b tcc__r0
  sta.b tcc__r2h

  ; X against solid cells: a wall stops the body flush against it. On the
  ; ground the feet's half tile is a step, measured where the feet will be.
loom_pvs_player_x:
  lda.b tcc__r1
  clc
  adc.b tcc__f2
  sta.b tcc__r3                         ; left
  clc
  adc.b tcc__f3
  dec a
  sta.b tcc__r3h                        ; right
  lda.b tcc__r1h
  clc
  adc.b tcc__f2h
  sta.b tcc__r4                         ; top
  clc
  adc.b tcc__f3h
  dec a
  sta.b tcc__r4h                        ; bottom
  sep #$20
  lda #0
  sta.l loom_movement_state+22          ; wall
  sta.l loom_movement_state+24          ; landed
  rep #$20
  lda.l loom_movement_state+4
  and #$00ff
  sta.l loom_pvs_actor_next_sub
  lda.b tcc__r2
  sta.b tcc__r0
  lda.b tcc__r1
  jsr loom_pvs_tight_advance
  sta.b tcc__r5h                        ; next_x
  cmp.b tcc__r1
  bne +
  brl loom_pvs_player_x_done
+ sec
  sbc.b tcc__r1
  sta.l loom_pvs_actor_delta
  bmi +
  lda.b tcc__r3h
  bra ++
+ lda.b tcc__r3
++ sta.l loom_pvs_actor_edge
  lda.b tcc__r4h
  sta.l loom_pvs_actor_wall_bottom
  lda.l loom_pvs_actor_grounded
  bne +
  brl loom_pvs_player_x_probe
+ lda.b tcc__r4h
  sta.l loom_pvs_player_climb
  lda.b tcc__f3
  lsr a
  clc
  adc.b tcc__f2
  clc
  adc.b tcc__r5h                        ; the sensor where the feet will be
  tax
  lda.b tcc__r4h
  jsr loom_pvs_player_slope_feet
  bcc +
  sta.b tcc__r0
  LOOM_SIGNED_CMP tcc__r4h
  beq ++
  bpl +                                 ; a floor below the feet: the feet
++ lda.b tcc__r0
  sta.l loom_pvs_player_climb
+ lda.l loom_pvs_player_climb
  sec
  sbc #8
  LOOM_SIGNED_CMP tcc__r4
  bpl ++
  lda.b tcc__r4                         ; never above the top
  bra +++
++ lda.l loom_pvs_player_climb
  sec
  sbc #8
+++ sta.l loom_pvs_actor_wall_bottom
loom_pvs_player_x_probe:
  jsr loom_pvs_tight_probe_x            ; r0 <- the blocking column or $7fff
  lda.b tcc__r0
  cmp #$7fff
  bne +
  brl loom_pvs_player_x_store
+ sec
  sbc.l loom_pvs_actor_edge
  clc
  adc.b tcc__r1
  sta.b tcc__r5h                        ; x + (probe - edge)
  lda.l loom_pvs_actor_delta
  bmi +
  dec.b tcc__r5h
  lda #2                                ; LOOM_MOVEMENT_WALL_RIGHT
  bra ++
+ inc.b tcc__r5h
  lda #1                                ; LOOM_MOVEMENT_WALL_LEFT
++ sep #$20
  sta.l loom_movement_state+22
  rep #$20
  lda #0
  sta.b tcc__r2                         ; vx = 0
  sta.l loom_pvs_actor_next_sub
loom_pvs_player_x_store:
  ; A solid actor is a wall as well; the body stays where it was.
  jsr loom_pvs_solids
  beq loom_pvs_player_x_place
  lda.b tcc__r5h
  cmp.b tcc__r1
  beq loom_pvs_player_x_place
  clc
  adc.b tcc__f2
  sta.b tcc__r0h                        ; the stop's left edge
  jsr loom_pvs_probe_actor_wall
  beq loom_pvs_player_x_place
  lda.b tcc__r1
  sta.b tcc__r5h                        ; stay
  lda.l loom_pvs_actor_delta
  bmi +
  lda #2
  bra ++
+ lda #1
++ sep #$20
  sta.l loom_movement_state+22
  rep #$20
  lda #0
  sta.b tcc__r2
  sta.l loom_pvs_actor_next_sub
loom_pvs_player_x_place:
  lda.b tcc__r5h
  sta.b tcc__r1
  clc
  adc.b tcc__f2
  sta.b tcc__r3
  clc
  adc.b tcc__f3
  dec a
  sta.b tcc__r3h
loom_pvs_player_x_done:
  lda.l loom_pvs_actor_next_sub
  sep #$20
  sta.l loom_movement_state+4           ; subpixel_x
  rep #$20
  lda.b tcc__f3
  lsr a
  clc
  adc.b tcc__r3
  sta.b tcc__r5                         ; sensor_x

  ; Y: falling lands on the first floor between the old feet and the new;
  ; rising stops under the first solid ceiling.
  lda.l loom_movement_state+5
  and #$00ff
  sta.l loom_pvs_actor_next_sub
  lda.b tcc__r2h
  sta.b tcc__r0
  lda.b tcc__r1h
  jsr loom_pvs_tight_advance
  sta.b tcc__r5h                        ; next_y
  lda.b tcc__r2h
  beq +
  bmi ++
  brl loom_pvs_player_fall
++ brl loom_pvs_player_rise
+ lda.l loom_pvs_actor_grounded
  beq +
  brl loom_pvs_player_fall
+ brl loom_pvs_player_free
loom_pvs_player_fall:
  lda.b tcc__r5h
  clc
  adc.b tcc__f2h
  clc
  adc.b tcc__f3h
  dec a
  sta.l loom_pvs_actor_reach            ; the new feet
  lda.l loom_pvs_actor_grounded
  beq +
  lda.l loom_pvs_actor_reach
  clc
  adc #16                               ; a slope or a lip stepped down
  sta.l loom_pvs_actor_reach
+ jsr loom_pvs_tight_scan_floor         ; landed
  sep #$20
  lda #$ff
  sta.l loom_movement_state+23          ; riding nothing, unless a platform
  rep #$20
  jsr loom_pvs_solids
  beq loom_pvs_player_slope
  jsr loom_pvs_probe_actor_floor
  cmp #$00ff
  beq loom_pvs_player_slope
  sta.b tcc__r0h                        ; the platform
  lda.l loom_pvs_actor_landed
  bmi +
  lda.l loom_movement_probe_floor
  LOOM_SIGNED_CMP_L loom_pvs_actor_landed
  bpl loom_pvs_player_slope             ; the tile floor is higher
+ lda.l loom_movement_probe_floor
  sta.l loom_pvs_actor_landed
  lda.b tcc__r0h
  sep #$20
  sta.l loom_movement_state+23          ; riding it
  rep #$20
loom_pvs_player_slope:
  ldx.b tcc__r5
  lda.b tcc__r4h
  jsr loom_pvs_player_slope_feet        ; the slope under the sensor
  bcc loom_pvs_player_snap
  sta.b tcc__r0
  lda.l loom_pvs_actor_landed
  bmi +
  lda.b tcc__r0
  LOOM_SIGNED_CMP_L loom_pvs_actor_landed
  bpl loom_pvs_player_snap              ; the floor found is higher
+ lda.b tcc__r0
  sta.l loom_pvs_actor_landed
  sep #$20
  lda #$ff
  sta.l loom_movement_state+23          ; a slope is ground
  rep #$20
loom_pvs_player_snap:
  lda.l loom_pvs_actor_landed
  bmi loom_pvs_player_fall_free
  LOOM_SIGNED_CMP_L loom_pvs_actor_reach
  beq +
  bpl loom_pvs_player_fall_free         ; below the reach
+ lda.l loom_pvs_actor_landed
  sec
  sbc.b tcc__f3h
  inc a
  sec
  sbc.b tcc__f2h
  sta.b tcc__r1h                        ; y on the floor
  lda #0
  sta.b tcc__r2h                        ; vy = 0
  sep #$20
  sta.l loom_movement_state+5           ; subpixel_y = 0
  lda.l loom_pvs_actor_grounded
  bne +
  lda #0
  sta.l loom_movement_state+21          ; jumping = 0
  lda #1
  sta.l loom_movement_state+24          ; landed
+ lda #1
  sta.l loom_movement_state+18          ; on_ground
  rep #$20
  brl loom_pvs_player_tail
loom_pvs_player_fall_free:
  lda.b tcc__r5h
  sta.b tcc__r1h
  lda.l loom_pvs_actor_next_sub
  sep #$20
  sta.l loom_movement_state+5
  rep #$20
  lda.l loom_pvs_actor_grounded
  beq +
  ldy #16
  lda [tcc__r10],y                      ; walked off an edge: coyote_ticks
  sep #$20
  sta.l loom_movement_state+19
  rep #$20
+ sep #$20
  lda #0
  sta.l loom_movement_state+18          ; on_ground = 0
  lda #$ff
  sta.l loom_movement_state+23
  rep #$20
  brl loom_pvs_player_tail
loom_pvs_player_rise:
  sep #$20
  lda #$ff
  sta.l loom_movement_state+23          ; riding
  lda #0
  sta.l loom_movement_state+18          ; on_ground = 0
  rep #$20
  lda.b tcc__r5h
  clc
  adc.b tcc__f2h
  sta.l loom_pvs_actor_reach            ; the new head
  jsr loom_pvs_tight_scan_ceiling       ; stopped
  lda.l loom_pvs_actor_stopped
  bmi loom_pvs_player_free
  sec
  sbc.b tcc__f2h
  sta.b tcc__r1h                        ; y under the ceiling
  lda #0
  sta.b tcc__r2h
  sep #$20
  sta.l loom_movement_state+5
  sta.l loom_movement_state+21          ; jumping = 0
  rep #$20
  brl loom_pvs_player_tail
loom_pvs_player_free:
  lda.b tcc__r5h
  sta.b tcc__r1h
  lda.l loom_pvs_actor_next_sub
  sep #$20
  sta.l loom_movement_state+5
  rep #$20
loom_pvs_player_tail:
  lda.l loom_pvs_actor_intent
  beq +
  lda #1
+ sep #$20
  sta.l loom_movement_state+10          ; moving
  rep #$20
  lda.l loom_movement_state+22
  and #$00ff
  beq +
  sep #$20
  lda #1
  sta.l loom_movement_state+8           ; last_collision = solid
  rep #$20
  lda.l loom_movement_state+6
  cmp #$ffff
  beq +
  inc a
  sta.l loom_movement_state+6           ; blocked_count
+ lda.b tcc__r1
  sta.l loom_movement_state+0
  lda.b tcc__r1h
  sta.l loom_movement_state+2
  lda.b tcc__r2
  sta.l loom_movement_state+14
  lda.b tcc__r2h
  sta.l loom_movement_state+16
  ; the sprite follows: Mode 1's tables straight, through the binding
  lda.l loom_movement_state+36
  sta.b tcc__r0
  lda.l loom_movement_state+38
  sta.b tcc__r0h
  ldy #22
  lda [tcc__r0],y
  and #$00ff                            ; player_slot
  tay
  lda.l loom_pvs_mode1_slot_index
  sta.b tcc__r0
  lda.l loom_pvs_mode1_slot_index+2
  sta.b tcc__r0h
  lda [tcc__r0],y
  and #$00ff
  cmp #$00ff
  beq +
  jsr loom_pvs_place_sprite
+ plp
  rtl

; The sprite at index A (and its metasprite parts after it) moves to x r1,
; y r1h in Mode 1's world tables. Clobbers r3, r4, r5, X, Y.
loom_pvs_place_sprite:
  sta.l loom_pvs_pass_cursor
  lda.l loom_pvs_mode1_part_count
  sta.b tcc__r3
  lda.l loom_pvs_mode1_part_count+2
  sta.b tcc__r3h
  lda.l loom_pvs_pass_cursor
  tay
  lda [tcc__r3],y
  and #$00ff
  sta.l loom_pvs_pass_remaining
  lda.l loom_pvs_mode1_world_x
  sta.b tcc__r4
  lda.l loom_pvs_mode1_world_x+2
  sta.b tcc__r4h
  lda.l loom_pvs_mode1_world_y
  sta.b tcc__r5
  lda.l loom_pvs_mode1_world_y+2
  sta.b tcc__r5h
- lda.l loom_pvs_pass_cursor
  asl a
  tay
  lda.b tcc__r1
  sta [tcc__r4],y
  lda.b tcc__r1h
  sta [tcc__r5],y
  lda.l loom_pvs_pass_remaining
  beq +
  dec a
  sta.l loom_pvs_pass_remaining
  lda.l loom_pvs_pass_cursor
  inc a
  cmp.l loom_pvs_mode1_sprite_count
  bcs +
  sta.l loom_pvs_pass_cursor
  bra -
+ rts

; The eased magnitude for a cap that dropped: speed - friction when the
; friction is nonzero and that stays over the cap, else the cap. A <- it.
; Clobbers r0, Y.
loom_pvs_player_ease:
  ldy #4
  lda [tcc__r10],y
  beq +
  sta.b tcc__r0
  lda.l loom_pvs_player_speed
  sec
  sbc.b tcc__r0
  sta.b tcc__r0                         ; speed - friction
  LOOM_SIGNED_CMP_L loom_pvs_player_cap
  beq +
  bmi +
  lda.b tcc__r0
  rts
+ lda.l loom_pvs_player_cap
  rts

; The slope under the ground sensor at column X with the feet at row A: the
; slope cell the sensor is in, or, when that cell is solid (climbing into
; the next slope tile), the one in the row above. Carry set with the feet
; row in A when a slope applies. Clobbers r0h, X, Y, loom_pvs_actor_row.
loom_pvs_player_slope_feet:
  sta.l loom_pvs_actor_tmp              ; feet
  txa
  sta.l loom_pvs_player_col
  lda.l loom_pvs_actor_tmp
  and #$fff0
  sta.l loom_pvs_actor_row
  lda.l loom_pvs_actor_tmp
  jsr loom_pvs_tight_cell
  cmp #1
  bne +
  lda.l loom_pvs_actor_row
  sec
  sbc #16
  sta.l loom_pvs_actor_row
  lda.l loom_pvs_player_col
  tax
  lda.l loom_pvs_actor_row
  jsr loom_pvs_tight_cell
+ cmp #3
  beq +
  cmp #4
  beq ++
  clc
  rts
+ lda.l loom_pvs_player_col
  and #$000f
  eor #$ffff
  sec
  adc.l loom_pvs_actor_row
  clc
  adc #15                               ; row + 15 - within
  sec
  rts
++ lda.l loom_pvs_player_col
  and #$000f
  clc
  adc.l loom_pvs_actor_row              ; row + within
  sec
  rts

; A <- ((A >> 4) * (speed >> 4)) / (top >> 4): the jump's scaling by the
; run, sixteen-bit unsigned with the product truncated as the C's is (top
; sits in loom_pvs_player_climb). Clobbers r0, r0h, X.
loom_pvs_player_jump_scale:
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r0
  lda.l loom_pvs_player_speed
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r0h
  jsr loom_pvs_player_umul
  sta.b tcc__r0
  lda.l loom_pvs_player_climb
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r0h
  jsr loom_pvs_player_udiv
  rts

; A <- (r0 * r0h) mod 65536. Clobbers r0, r0h, X.
loom_pvs_player_umul:
  lda #0
  ldx #16
- lsr.b tcc__r0h
  bcc +
  clc
  adc.b tcc__r0
+ asl.b tcc__r0
  dex
  bne -
  rts

; A <- r0 / r0h, unsigned; zero for a zero divisor. Clobbers r0, X.
loom_pvs_player_udiv:
  lda.b tcc__r0h
  bne +
  lda #0
  rts
+ lda #0
  ldx #16
- asl.b tcc__r0
  rol a
  cmp.b tcc__r0h
  bcc +
  sbc.b tcc__r0h
  inc.b tcc__r0
+ dex
  bne -
  lda.b tcc__r0
  rts

; ---------------------------------------------------------------------------
; The actor pass (PERF-004): loom_actor_update's loop for the common actor --
; a live, awake patrol or bounce walker with a platformer body in a pool
; without solid actors -- with the patrol, the still check, the tight step, the sprite
; position and the animation key inline. Every other actor, and every
; transition (waking, falling asleep, arriving at a waypoint, a projectile
; leaving play), goes on the slow list for the C loop body, which then does
; the whole of that actor's tick itself: nothing here mutates an actor
; before deciding it is fast.
;
; The pool's arrays are bound once per activation (loom_pvs_actor_bind and
; loom_pvs_actor_bind_pass), Mode 1's sprite tables by loom_pvs_mode1_bind.
; Field offsets follow 816-tcc's layout: LoomActorType body 32, collision
; 33, behavior 34, flags 35 (42 bytes); LoomActorInstance first_waypoint 4,
; waypoint_count 6 (10 bytes); LoomActorWaypoint x 0, y 2 (4 bytes); all
; asserted in actor.c.
;
; The loop's state lives in RAM (long addressing): a C callout clobbers the
; direct page, which is scratch per actor here.

; dest/desth <- the pointer in the pointer array at idx4.
.MACRO LOOM_ACTOR_LOAD_PTR ARGS array, dest, desth
  lda.l array
  sta.b tcc__r0
  lda.l array+2
  sta.b tcc__r0h
  lda.l loom_pvs_actor_idx4
  tay
  lda [tcc__r0],y
  sta.b dest
  iny
  iny
  lda [tcc__r0],y
  sta.b desth
.ENDM

; void loom_pvs_actor_bind_pass(alive 5,s; inert 9,s; awake 13,s; type_ptr
;   17,s; body_ptr 21,s; behavior_a 25,s; behavior_b 29,s; blocked 33,s;
;   sprite_index 37,s; sprite_sent_x 41,s; sprite_sent_y 45,s;
;   animation_sent 49,s; instances 53,s; waypoints 57,s)
loom_pvs_actor_bind_pass:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_actor_arr_alive
  lda 7,s
  sta.l loom_pvs_actor_arr_alive+2
  lda 9,s
  sta.l loom_pvs_actor_arr_inert
  lda 11,s
  sta.l loom_pvs_actor_arr_inert+2
  lda 13,s
  sta.l loom_pvs_actor_arr_awake
  lda 15,s
  sta.l loom_pvs_actor_arr_awake+2
  lda 17,s
  sta.l loom_pvs_actor_arr_type_ptr
  lda 19,s
  sta.l loom_pvs_actor_arr_type_ptr+2
  lda 21,s
  sta.l loom_pvs_actor_arr_body_ptr
  lda 23,s
  sta.l loom_pvs_actor_arr_body_ptr+2
  lda 25,s
  sta.l loom_pvs_actor_arr_behavior_a
  lda 27,s
  sta.l loom_pvs_actor_arr_behavior_a+2
  lda 29,s
  sta.l loom_pvs_actor_arr_behavior_b
  lda 31,s
  sta.l loom_pvs_actor_arr_behavior_b+2
  lda 33,s
  sta.l loom_pvs_actor_arr_blocked
  lda 35,s
  sta.l loom_pvs_actor_arr_blocked+2
  lda 37,s
  sta.l loom_pvs_actor_arr_sprite_index
  lda 39,s
  sta.l loom_pvs_actor_arr_sprite_index+2
  lda 41,s
  sta.l loom_pvs_actor_arr_sent_x
  lda 43,s
  sta.l loom_pvs_actor_arr_sent_x+2
  lda 45,s
  sta.l loom_pvs_actor_arr_sent_y
  lda 47,s
  sta.l loom_pvs_actor_arr_sent_y+2
  lda 49,s
  sta.l loom_pvs_actor_arr_animation_sent
  lda 51,s
  sta.l loom_pvs_actor_arr_animation_sent+2
  lda 53,s
  sta.l loom_pvs_actor_instances
  lda 55,s
  sta.l loom_pvs_actor_instances+2
  lda 57,s
  sta.l loom_pvs_actor_waypoints
  lda 59,s
  sta.l loom_pvs_actor_waypoints+2
  plp
  rtl

; void loom_pvs_mode1_bind(world_x 5,s; world_y 9,s; part_count 13,s; sprite_count
;   17,s; slot_index 19,s; camera 23,s (x, then y); camera_min_x 27,s;
;   camera_min_y 29,s; camera_max_x 31,s; camera_max_y 33,s)
loom_pvs_mode1_bind:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_mode1_world_x
  lda 7,s
  sta.l loom_pvs_mode1_world_x+2
  lda 9,s
  sta.l loom_pvs_mode1_world_y
  lda 11,s
  sta.l loom_pvs_mode1_world_y+2
  lda 13,s
  sta.l loom_pvs_mode1_part_count
  lda 15,s
  sta.l loom_pvs_mode1_part_count+2
  lda 17,s
  sta.l loom_pvs_mode1_sprite_count
  lda 19,s
  sta.l loom_pvs_mode1_slot_index
  lda 21,s
  sta.l loom_pvs_mode1_slot_index+2
  lda 23,s
  sta.l loom_pvs_mode1_camera
  lda 25,s
  sta.l loom_pvs_mode1_camera+2
  lda 27,s
  sta.l loom_pvs_mode1_cmin_x
  lda 29,s
  sta.l loom_pvs_mode1_cmin_y
  lda 31,s
  sta.l loom_pvs_mode1_cmax_x
  lda 33,s
  sta.l loom_pvs_mode1_cmax_y
  plp
  rtl

; The spawn window comes from Mode 1's camera through the binding: the view,
; widened by LOOM_ACTOR_SPAWN_WINDOW_MARGIN (64) on every side.
; void loom_pvs_actor_pass(loom_u16 count 5,s; loom_u8 *slow 7,s;
;                          loom_u8 *out 11,s)
; out[0] <- the slow list's length, out[1] <- the sleepers counted here,
; out[2] <- 1 when a fast actor was stepped.
loom_pvs_actor_pass:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_pass_count
  lda 7,s
  sta.l loom_pvs_pass_slow
  lda 9,s
  sta.l loom_pvs_pass_slow+2
  lda 11,s
  sta.l loom_pvs_pass_out
  lda 13,s
  sta.l loom_pvs_pass_out+2
  lda.l loom_pvs_mode1_camera
  sta.b tcc__r0
  lda.l loom_pvs_mode1_camera+2
  sta.b tcc__r0h
  ldy #0
  lda [tcc__r0],y
  sec
  sbc #64
  sta.l loom_pvs_pass_left
  clc
  adc #256+128
  sta.l loom_pvs_pass_right
  ldy #2
  lda [tcc__r0],y
  sec
  sbc #64
  sta.l loom_pvs_pass_top
  clc
  adc #224+128
  sta.l loom_pvs_pass_bottom
  lda #0
  sta.l loom_pvs_pass_slow_count
  sta.l loom_pvs_pass_sleeping
  sta.l loom_pvs_pass_stepped
  sta.l loom_pvs_actor_idx
loom_pvs_pass_loop:
  lda.l loom_pvs_actor_idx
  cmp.l loom_pvs_pass_count
  bcc +
  brl loom_pvs_pass_done
+ asl a
  sta.l loom_pvs_actor_idx2
  asl a
  sta.l loom_pvs_actor_idx4
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_alive
  bne +
  brl loom_pvs_pass_next
+ LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_inert
  beq +
  brl loom_pvs_pass_next
+ ; the idle second player: nothing to do until its pad is pressed
  lda.l loom_actor_controller_idle
  and #$00ff
  beq +
  lda.l loom_actor_controller_slot
  and #$00ff
  cmp.l loom_pvs_actor_idx
  bne +
  brl loom_pvs_pass_next
+ LOOM_ACTOR_LOAD_PTR loom_pvs_actor_arr_type_ptr tcc__r3 tcc__r3h
  ldy #34
  lda [tcc__r3],y
  and #$00ff
  sta.l loom_pvs_pass_behavior
  lda #0
  sta.l loom_pvs_pass_topdown
  ldy #32
  lda [tcc__r3],y
  and #$00ff
  cmp #1                                ; LOOM_ACTOR_BODY_TOP_DOWN
  bne loom_pvs_pass_platformer
  ; A top-down patrol or chase steps here (loom_pvs_pass_topdown) unless the
  ; room has static colliders, which only the C box test knows.
  lda.l loom_movement_static_colliders
  and #$00ff
  beq +
  brl loom_pvs_pass_slow_actor
+ lda.l loom_pvs_pass_behavior
  cmp #1                                ; LOOM_ACTOR_BEHAVIOR_PATROL
  beq +
  cmp #4                                ; LOOM_ACTOR_BEHAVIOR_CHASE
  beq +
  brl loom_pvs_pass_slow_actor
+ lda #1
  sta.l loom_pvs_pass_topdown
  brl loom_pvs_pass_window
loom_pvs_pass_platformer:
  lda.l loom_pvs_pass_behavior
  cmp #1                                ; LOOM_ACTOR_BEHAVIOR_PATROL
  beq +
  cmp #3                                ; LOOM_ACTOR_BEHAVIOR_BOUNCE
  beq +
  brl loom_pvs_pass_slow_actor
+ ldy #33
  lda [tcc__r3],y
  and #$00ff
  cmp #2                                ; LOOM_ACTOR_COLLISION_SOLID carries riders
  bne +
  brl loom_pvs_pass_slow_actor
+ ldy #32
  lda [tcc__r3],y
  and #$00ff
  cmp #2                                ; LOOM_ACTOR_BODY_PLATFORMER
  beq +
  brl loom_pvs_pass_slow_actor
+ LOOM_ACTOR_LOAD_PTR loom_pvs_actor_arr_body_ptr tcc__r10 tcc__r10h
  lda.b tcc__r10
  ora.b tcc__r10h
  bne +
  brl loom_pvs_pass_slow_actor
+
loom_pvs_pass_window:
  ; the spawn window
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_x tcc__r1
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_y tcc__r1h
  lda.b tcc__r1
  LOOM_SIGNED_CMP_L loom_pvs_pass_left
  bmi loom_pvs_pass_outside
  lda.l loom_pvs_pass_right
  LOOM_SIGNED_CMP tcc__r1
  bmi loom_pvs_pass_outside             ; right < x
  lda.b tcc__r1h
  LOOM_SIGNED_CMP_L loom_pvs_pass_top
  bmi loom_pvs_pass_outside
  lda.l loom_pvs_pass_bottom
  LOOM_SIGNED_CMP tcc__r1h
  bmi loom_pvs_pass_outside             ; bottom < y
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_awake
  bne loom_pvs_pass_fast
  brl loom_pvs_pass_slow_actor          ; waking: the C loop starts its clip
loom_pvs_pass_outside:
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_awake
  beq +
  brl loom_pvs_pass_slow_actor          ; falling asleep: the C loop stops its clip
+ lda.l loom_pvs_pass_sleeping
  inc a
  sta.l loom_pvs_pass_sleeping
  brl loom_pvs_pass_next

loom_pvs_pass_fast:
  lda.l loom_pvs_pass_topdown
  beq +
  brl loom_pvs_pass_topdown_step
+ lda.l loom_pvs_pass_behavior
  cmp #3
  beq +
  brl loom_pvs_pass_patrol
+ ; Bounce: the first tick starts along the axes the type bounces on; a
  ; blocked axis reverses.
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_x
  sta.l loom_pvs_pass_ix
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_y
  ora.l loom_pvs_pass_ix
  bne +
  ldy #35
  lda [tcc__r3],y                       ; the type's flags
  sta.l loom_pvs_pass_tmp
  and #$0002                            ; LOOM_ACTOR_FLAG_BOUNCE_X
  beq ++
  lda #1
++ LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_x
  lda.l loom_pvs_pass_tmp
  and #$0004                            ; LOOM_ACTOR_FLAG_BOUNCE_Y
  beq ++
  lda #1
++ LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_y
  brl loom_pvs_pass_still
+ LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_blocked
  sta.l loom_pvs_pass_tmp
  and #$0001
  beq +
  lda.l loom_pvs_pass_ix
  eor #$00ff
  inc a
  and #$00ff
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_x
+ lda.l loom_pvs_pass_tmp
  and #$0002
  beq +
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_y
  eor #$00ff
  inc a
  and #$00ff
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_y
+ brl loom_pvs_pass_still
loom_pvs_pass_patrol:
  ; Patrol: waiting at a waypoint counts down with no intent; else walk
  ; toward the current waypoint along x (gravity owns y). Arriving is the
  ; C loop's: it picks the next leg and the ping-pong turn.
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_behavior_b tcc__r2
  lda.b tcc__r2
  beq +
  dec a
  sta.b tcc__r2
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_behavior_b tcc__r2
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_x
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_y
  brl loom_pvs_pass_still
+ lda.l loom_pvs_actor_idx
  asl a
  asl a
  clc
  adc.l loom_pvs_actor_idx
  asl a                                 ; idx * 10
  clc
  adc.l loom_pvs_actor_instances
  sta.b tcc__r2
  lda.l loom_pvs_actor_instances+2
  sta.b tcc__r2h                        ; the instance
  ldy #6
  lda [tcc__r2],y
  and #$00ff
  sta.b tcc__r4h                        ; waypoint_count
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_behavior_a
  cmp.b tcc__r4h
  bcc +
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_behavior_a
  lda #0
+ sta.b tcc__r4                         ; step
  ldy #4
  lda [tcc__r2],y                       ; first_waypoint
  clc
  adc.b tcc__r4
  asl a
  asl a
  clc
  adc.l loom_pvs_actor_waypoints
  sta.b tcc__r5
  lda.l loom_pvs_actor_waypoints+2
  sta.b tcc__r5h                        ; the target waypoint
  ldy #0
  lda [tcc__r5],y
  sec
  sbc.b tcc__r1                         ; delta_x
  bne +
  brl loom_pvs_pass_slow_actor          ; arrived
+ bmi +
  lda #1
  bra ++
+ lda #$00ff
++ LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_x
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_y

loom_pvs_pass_still:
  ; An actor standing still on solid ground costs no step.
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_x
  beq +
  brl loom_pvs_pass_step
+ LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_y
  beq +
  brl loom_pvs_pass_step
+ LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_vx tcc__r5
  lda.b tcc__r5
  beq +
  brl loom_pvs_pass_step
+ LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_vy tcc__r5
  lda.b tcc__r5
  beq +
  brl loom_pvs_pass_step
+ LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_riding
  cmp #$00ff
  beq +
  brl loom_pvs_pass_step
+ LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_body_flags
  bit #$0001
  bne +
  brl loom_pvs_pass_step
+ and #$00f7                            ; the landed flag is spent
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_body_flags
  brl loom_pvs_pass_sprite
loom_pvs_pass_step:
  ; The tight step: the body in r10, the box in f2/f3.
  LOOM_ACTOR_LOAD_PTR loom_pvs_actor_arr_type_ptr tcc__r3 tcc__r3h
  ldy #2
  lda [tcc__r3],y
  sta.b tcc__f2
  ldy #4
  lda [tcc__r3],y
  sta.b tcc__f2h
  ldy #6
  lda [tcc__r3],y
  sta.b tcc__f3
  ldy #8
  lda [tcc__r3],y
  sta.b tcc__f3h
  LOOM_ACTOR_LOAD_PTR loom_pvs_actor_arr_body_ptr tcc__r10 tcc__r10h
  jsr loom_pvs_tight_core               ; r0 <- the body flags
  lda.b tcc__r0
  and #$0006
  beq +
  lda #1
+ sta.l loom_pvs_pass_tmp
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_blocked
  and #$0080
  ora.l loom_pvs_pass_tmp
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_blocked

loom_pvs_pass_sprite:
  ; The sprite follows a position that changed, its metasprite parts with it.
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_x tcc__r1
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_y tcc__r1h
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_sent_x tcc__r2
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_sent_y tcc__r2h
  lda.b tcc__r1
  cmp.b tcc__r2
  bne +
  lda.b tcc__r1h
  cmp.b tcc__r2h
  bne +
  brl loom_pvs_pass_key
+ LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_sent_x tcc__r1
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_sent_y tcc__r1h
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_sprite_index
  cmp #$00ff
  bne +
  brl loom_pvs_pass_key
+ jsr loom_pvs_place_sprite

loom_pvs_pass_key:
  ; The animation key: moving | (intent_x + 1) << 1 | (intent_y + 1) << 3 |
  ; air << 5, driven through C only when it changed.
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_body_flags
  sta.l loom_pvs_pass_tmp
  bit #$0008
  beq +
  lda #3                                ; landed
  bra loom_pvs_pass_air
+ bit #$0001
  bne ++
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_vy tcc__r5
  lda.b tcc__r5
  bmi +
  lda #2                                ; falling
  bra loom_pvs_pass_air
+ lda #1                                ; rising
  bra loom_pvs_pass_air
++ lda #0                               ; on the ground
loom_pvs_pass_air:
  sta.l loom_pvs_pass_airv
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_x
  sta.l loom_pvs_pass_ix
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_y
  sta.l loom_pvs_pass_iy
  ora.l loom_pvs_pass_ix
  beq +
  lda #1
+ sta.l loom_pvs_pass_keyv
  lda.l loom_pvs_pass_ix
  inc a
  and #$00ff
  asl a
  ora.l loom_pvs_pass_keyv
  sta.l loom_pvs_pass_keyv
  lda.l loom_pvs_pass_iy
  inc a
  and #$00ff
  asl a
  asl a
  asl a
  ora.l loom_pvs_pass_keyv
  sta.l loom_pvs_pass_keyv
  lda.l loom_pvs_pass_airv
  asl a
  asl a
  asl a
  asl a
  asl a
  ora.l loom_pvs_pass_keyv
  and #$00ff
  sta.l loom_pvs_pass_keyv
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_animation_sent
  cmp.l loom_pvs_pass_keyv
  beq +
  ; loom_actor_drive_slot(index, key, air): 816-tcc's convention, the last
  ; argument pushed first, the caller dropping them after.
  lda.l loom_pvs_pass_airv
  pha
  lda.l loom_pvs_pass_keyv
  pha
  lda.l loom_pvs_actor_idx
  pha
  jsl loom_actor_drive_slot
  rep #$30
  tsc
  clc
  adc #6
  tcs
+ lda #1
  sta.l loom_pvs_pass_stepped
  brl loom_pvs_pass_next

; ---------------------------------------------------------------------------
; A top-down actor's tick (PERF-004): loom_actor_update_slot's patrol and
; chase with a top-down body -- the behaviour's intent, then x and then y
; stepped by the type's speed and stopped by any collision cell under the box
; (loom_movement_box_blocked while the C resolve flag is set: tiles only, no
; solid actors, and no static colliders, which send the actor to the C loop
; instead), the riders carried when a solid actor moved, the sprite, and the
; animation key with the air state always the ground. Arriving at a waypoint
; is the C loop's, as it is for walkers. The type stays in r3 until the
; sprite is placed.
loom_pvs_pass_topdown_step:
  LOOM_ACTOR_LOAD_PTR loom_pvs_actor_arr_type_ptr tcc__r3 tcc__r3h
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_x tcc__r1
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_y tcc__r1h
  lda.b tcc__r1
  sta.l loom_pvs_td_x
  sta.l loom_pvs_td_x_before
  lda.b tcc__r1h
  sta.l loom_pvs_td_y
  sta.l loom_pvs_td_y_before
  lda.l loom_pvs_pass_behavior
  cmp #4
  beq loom_pvs_td_chase
  brl loom_pvs_td_patrol
loom_pvs_td_chase:
  ; Toward the player while it is within reach on the larger axis; the
  ; sight and losing ranges give the interest its hysteresis.
  lda.l loom_movement_state             ; player_x
  sec
  sbc.b tcc__r1
  sta.b tcc__r2                         ; delta_x
  lda.l loom_movement_state+2           ; player_y
  sec
  sbc.b tcc__r1h
  sta.b tcc__r2h                        ; delta_y
  lda.b tcc__r2
  bpl loom_pvs_td_chase_ax
  eor #$ffff
  inc a
loom_pvs_td_chase_ax:
  sta.b tcc__r4
  lda.b tcc__r2h
  bpl loom_pvs_td_chase_ay
  eor #$ffff
  inc a
loom_pvs_td_chase_ay:
  cmp.b tcc__r4
  bcc loom_pvs_td_chase_reach
  sta.b tcc__r4                         ; reach = the larger
loom_pvs_td_chase_reach:
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_behavior_a
  beq loom_pvs_td_chase_sight
  ldy #14                               ; behavior_lose
  bra loom_pvs_td_chase_range
loom_pvs_td_chase_sight:
  ldy #12                               ; behavior_sight
loom_pvs_td_chase_range:
  lda [tcc__r3],y
  cmp.b tcc__r4
  bcc +
  brl loom_pvs_td_chase_in              ; reach <= range
+
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_behavior_a
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_x
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_y
  brl loom_pvs_td_move
loom_pvs_td_chase_in:
  lda #1
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_behavior_a
  brl loom_pvs_td_intents

loom_pvs_td_patrol:
  ; Waiting at a waypoint counts down with no intent; else toward the
  ; current waypoint on both axes.
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_behavior_b tcc__r2
  lda.b tcc__r2
  beq loom_pvs_td_patrol_leg
  dec a
  sta.b tcc__r2
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_behavior_b tcc__r2
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_x
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_y
  brl loom_pvs_td_move
loom_pvs_td_patrol_leg:
  lda.l loom_pvs_actor_idx
  asl a
  asl a
  clc
  adc.l loom_pvs_actor_idx
  asl a                                 ; idx * 10
  clc
  adc.l loom_pvs_actor_instances
  sta.b tcc__r2
  lda.l loom_pvs_actor_instances+2
  sta.b tcc__r2h                        ; the instance
  ldy #6
  lda [tcc__r2],y
  and #$00ff
  sta.b tcc__r4h                        ; waypoint_count
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_behavior_a
  cmp.b tcc__r4h
  bcc loom_pvs_td_patrol_step
  lda #0
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_behavior_a
  lda #0
loom_pvs_td_patrol_step:
  sta.b tcc__r4                         ; step
  ldy #4
  lda [tcc__r2],y                       ; first_waypoint
  clc
  adc.b tcc__r4
  asl a
  asl a
  clc
  adc.l loom_pvs_actor_waypoints
  sta.b tcc__r5
  lda.l loom_pvs_actor_waypoints+2
  sta.b tcc__r5h                        ; the target waypoint
  ldy #0
  lda [tcc__r5],y
  sec
  sbc.b tcc__r1
  sta.b tcc__r2                         ; delta_x
  ldy #2
  lda [tcc__r5],y
  sec
  sbc.b tcc__r1h
  sta.b tcc__r2h                        ; delta_y
  ora.b tcc__r2
  beq +
  brl loom_pvs_td_intents
+ brl loom_pvs_pass_slow_actor          ; arrived

loom_pvs_td_intents:
  ; intent = the sign of delta_x (r2) and of delta_y (r2h)
  lda.b tcc__r2
  beq loom_pvs_td_intent_x
  bmi loom_pvs_td_intent_x_neg
  lda #1
  bra loom_pvs_td_intent_x
loom_pvs_td_intent_x_neg:
  lda #$00ff
loom_pvs_td_intent_x:
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_x
  lda.b tcc__r2h
  beq loom_pvs_td_intent_y
  bmi loom_pvs_td_intent_y_neg
  lda #1
  bra loom_pvs_td_intent_y
loom_pvs_td_intent_y_neg:
  lda #$00ff
loom_pvs_td_intent_y:
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_intent_y

loom_pvs_td_move:
  ; The blocked bits (1 x, 2 y) gather in pass_tmp. A top-down body has no
  ; velocity to spend and no landing to clear, so no intent is no step.
  lda #0
  sta.l loom_pvs_pass_tmp
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_x
  bne +
  brl loom_pvs_td_move_y
+ sta.l loom_pvs_td_dir
  lda.l loom_pvs_td_x
  sta.l loom_pvs_td_pos
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_sub_x
  sta.l loom_pvs_td_sub
  jsr loom_pvs_td_step
  jsr loom_pvs_td_boxed
  bcc +
  brl loom_pvs_td_move_x_free
+
  ; the box at the next x, the current y
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_sub_y
  sta.l loom_pvs_td_other_sub
  lda.l loom_pvs_td_next
  ldx #2
  jsr loom_pvs_td_span                  ; left, right from next x
  sta.l loom_pvs_td_left
  stx.b tcc__r4
  lda.l loom_pvs_td_next_sub
  beq +
  inc.b tcc__r4
+ lda.b tcc__r4
  sta.l loom_pvs_td_right
  lda.l loom_pvs_td_y
  ldx #4
  jsr loom_pvs_td_span                  ; top, bottom from the current y
  sta.l loom_pvs_td_top
  stx.b tcc__r4
  lda.l loom_pvs_td_other_sub
  beq +
  inc.b tcc__r4
+ lda.b tcc__r4
  sta.l loom_pvs_td_bottom
  jsr loom_pvs_td_box
  bcc loom_pvs_td_move_x_free
  lda #1
  sta.l loom_pvs_pass_tmp
  bra loom_pvs_td_move_y
loom_pvs_td_move_x_free:
  lda.l loom_pvs_td_next
  sta.l loom_pvs_td_x
  sta.b tcc__r1
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_x tcc__r1
  lda.l loom_pvs_td_next_sub
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_sub_x
loom_pvs_td_move_y:
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_intent_y
  bne +
  brl loom_pvs_td_moved
+ sta.l loom_pvs_td_dir
  lda.l loom_pvs_td_y
  sta.l loom_pvs_td_pos
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_sub_y
  sta.l loom_pvs_td_sub
  jsr loom_pvs_td_step
  jsr loom_pvs_td_boxed
  bcc +
  brl loom_pvs_td_move_y_free
+
  ; the box at the (moved) x, the next y
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_sub_x
  sta.l loom_pvs_td_other_sub
  lda.l loom_pvs_td_x
  ldx #2
  jsr loom_pvs_td_span
  sta.l loom_pvs_td_left
  stx.b tcc__r4
  lda.l loom_pvs_td_other_sub
  beq +
  inc.b tcc__r4
+ lda.b tcc__r4
  sta.l loom_pvs_td_right
  lda.l loom_pvs_td_next
  ldx #4
  jsr loom_pvs_td_span
  sta.l loom_pvs_td_top
  stx.b tcc__r4
  lda.l loom_pvs_td_next_sub
  beq +
  inc.b tcc__r4
+ lda.b tcc__r4
  sta.l loom_pvs_td_bottom
  jsr loom_pvs_td_box
  bcc loom_pvs_td_move_y_free
  lda.l loom_pvs_pass_tmp
  ora #2
  sta.l loom_pvs_pass_tmp
  bra loom_pvs_td_moved
loom_pvs_td_move_y_free:
  lda.l loom_pvs_td_next
  sta.l loom_pvs_td_y
  sta.b tcc__r1h
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_y tcc__r1h
  lda.l loom_pvs_td_next_sub
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_sub_y

loom_pvs_td_moved:
  ; Patrol keeps its direction flag in the high bit.
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_blocked
  and #$0080
  ora.l loom_pvs_pass_tmp
  LOOM_ACTOR_STORE_BYTE loom_pvs_actor_arr_blocked
  ; A solid actor that moved carries its riders, the player among them:
  ; loom_actor_carry_riders(index, delta_x, delta_y), 816-tcc's convention.
  ldy #33
  lda [tcc__r3],y
  and #$00ff
  cmp #2                                ; LOOM_ACTOR_COLLISION_SOLID
  beq +
  brl loom_pvs_td_sprite
+
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_x tcc__r1
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_y tcc__r1h
  lda.b tcc__r1h
  sec
  sbc.l loom_pvs_td_y_before
  sta.b tcc__r2h
  lda.b tcc__r1
  sec
  sbc.l loom_pvs_td_x_before
  sta.b tcc__r2
  ora.b tcc__r2h
  bne +
  brl loom_pvs_td_sprite
+ lda.b tcc__r2h
  pha
  lda.b tcc__r2
  pha
  lda.l loom_pvs_actor_idx
  pha
  jsl loom_actor_carry_riders
  rep #$30
  tsc
  clc
  adc #6
  tcs

loom_pvs_td_sprite:
  ; The sprite follows a position that changed; the air state is the ground.
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_x tcc__r1
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_y tcc__r1h
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_sent_x tcc__r2
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_sent_y tcc__r2h
  lda.b tcc__r1
  cmp.b tcc__r2
  bne +
  lda.b tcc__r1h
  cmp.b tcc__r2h
  bne +
  brl loom_pvs_td_key
+ LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_sent_x tcc__r1
  LOOM_ACTOR_STORE_WORD loom_pvs_actor_arr_sent_y tcc__r1h
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_sprite_index
  cmp #$00ff
  bne +
  brl loom_pvs_td_key
+ jsr loom_pvs_place_sprite
loom_pvs_td_key:
  lda #0
  brl loom_pvs_pass_air

; td_next/td_next_sub <- td_pos/td_sub stepped by the type's speed (r3 + 0)
; in direction td_dir (1 or $ff): loom_movement_step. Clobbers r4, r4h.
loom_pvs_td_step:
  ldy #0
  lda [tcc__r3],y
  and #$00ff
  sta.b tcc__r4h                        ; fraction
  lda [tcc__r3],y
  xba
  and #$00ff
  sta.b tcc__r4                         ; whole
  lda.l loom_pvs_td_dir
  cmp #1
  bne loom_pvs_td_step_back
  lda.l loom_pvs_td_sub
  clc
  adc.b tcc__r4h                        ; combined, 0..510
  pha
  and #$00ff
  sta.l loom_pvs_td_next_sub
  pla
  xba
  and #$00ff                            ; combined >> 8
  clc
  adc.b tcc__r4
  clc
  adc.l loom_pvs_td_pos
  sta.l loom_pvs_td_next
  rts
loom_pvs_td_step_back:
  lda.l loom_pvs_td_sub
  sec
  sbc.b tcc__r4h
  and #$00ff
  sta.l loom_pvs_td_next_sub
  lda.l loom_pvs_td_sub
  cmp.b tcc__r4h                        ; carry clear: a borrow
  lda #0
  rol a
  eor #1                                ; borrow
  clc
  adc.b tcc__r4                         ; delta = whole + borrow
  sta.b tcc__r4h
  lda.l loom_pvs_td_pos
  sec
  sbc.b tcc__r4h
  sta.l loom_pvs_td_next
  rts

; Carry set when the type has no box (width or height zero), so nothing
; stops it; clear when it has one.
loom_pvs_td_boxed:
  ldy #6
  lda [tcc__r3],y
  beq +
  ldy #8
  lda [tcc__r3],y
  beq +
  clc
  rts
+ sec
  rts

; A <- position + the box offset at r3 + X (2 for x, 4 for y); X <- that
; + the size at r3 + X + 4 - 1: a span's first and last pixel.
loom_pvs_td_span:
  sta.b tcc__r4
  txy
  lda [tcc__r3],y
  clc
  adc.b tcc__r4
  sta.b tcc__r4
  iny
  iny
  iny
  iny
  lda [tcc__r3],y
  clc
  adc.b tcc__r4
  dec a
  tax
  lda.b tcc__r4
  rts

; Carry set when any collision cell under td_left..td_right, td_top..
; td_bottom is not empty, or the box leaves the room. Clobbers X, Y, r0,
; r9, r10.
loom_pvs_td_box:
  jsr loom_pvs_body_grid
  lda.l loom_pvs_td_top
  sta.l loom_pvs_td_row
loom_pvs_td_box_row:
  lda.l loom_pvs_td_left
  sta.l loom_pvs_td_col
loom_pvs_td_box_col:
  lda.l loom_pvs_td_col
  tax
  lda.l loom_pvs_td_row
  jsr loom_pvs_body_cell
  bne loom_pvs_td_box_hit
  lda.l loom_pvs_td_col
  ora #$000f
  inc a
  sta.l loom_pvs_td_col
  lda.l loom_pvs_td_right
  LOOM_SIGNED_CMP_L loom_pvs_td_col
  bpl loom_pvs_td_box_col
  lda.l loom_pvs_td_row
  ora #$000f
  inc a
  sta.l loom_pvs_td_row
  lda.l loom_pvs_td_bottom
  LOOM_SIGNED_CMP_L loom_pvs_td_row
  bpl loom_pvs_td_box_row
  clc
  rts
loom_pvs_td_box_hit:
  sec
  rts


; ---------------------------------------------------------------------------
; loom_u8 loom_actor_controller_position(loom_s16 *x 5,s; loom_s16 *y 9,s)
; (PERF-004): the second player's position while it has joined -- its slot
; live and its behaviour started -- as actor.c's C does on the host. The
; slot is only ever valid once the pool is bound. r0 <- 1 with *x, *y set,
; else 0.
loom_actor_controller_position:
  php
  rep #$30
  lda.l loom_actor_controller_slot
  and #$00ff
  cmp #$00ff
  beq loom_pvs_controller_none
  sta.b tcc__r1
  tay
  lda.l loom_pvs_actor_arr_alive
  sta.b tcc__r0
  lda.l loom_pvs_actor_arr_alive+2
  sta.b tcc__r0h
  lda [tcc__r0],y
  and #$00ff
  beq loom_pvs_controller_none
  lda.l loom_pvs_actor_arr_behavior_a
  sta.b tcc__r0
  lda.l loom_pvs_actor_arr_behavior_a+2
  sta.b tcc__r0h
  lda [tcc__r0],y
  and #$00ff
  beq loom_pvs_controller_none
  lda.b tcc__r1
  asl a
  tay
  lda.l loom_pvs_actor_arr_x
  sta.b tcc__r0
  lda.l loom_pvs_actor_arr_x+2
  sta.b tcc__r0h
  lda [tcc__r0],y
  sta.b tcc__r2
  lda.l loom_pvs_actor_arr_y
  sta.b tcc__r0
  lda.l loom_pvs_actor_arr_y+2
  sta.b tcc__r0h
  lda [tcc__r0],y
  sta.b tcc__r2h
  lda 5,s
  sta.b tcc__r0
  lda 7,s
  sta.b tcc__r0h
  lda.b tcc__r2
  sta [tcc__r0]
  lda 9,s
  sta.b tcc__r0
  lda 11,s
  sta.b tcc__r0h
  lda.b tcc__r2h
  sta [tcc__r0]
  lda #1
  sta.b tcc__r0
  plp
  rtl
loom_pvs_controller_none:
  stz.b tcc__r0
  plp
  rtl

loom_pvs_pass_slow_actor:
  lda.l loom_pvs_pass_slow
  sta.b tcc__r0
  lda.l loom_pvs_pass_slow+2
  sta.b tcc__r0h
  lda.l loom_pvs_pass_slow_count
  tay
  inc a
  sta.l loom_pvs_pass_slow_count
  lda.l loom_pvs_actor_idx
  sep #$20
  sta [tcc__r0],y
  rep #$20
loom_pvs_pass_next:
  lda.l loom_pvs_actor_idx
  inc a
  sta.l loom_pvs_actor_idx
  brl loom_pvs_pass_loop
loom_pvs_pass_done:
  lda.l loom_pvs_pass_out
  sta.b tcc__r0
  lda.l loom_pvs_pass_out+2
  sta.b tcc__r0h
  sep #$20
  lda.l loom_pvs_pass_slow_count
  sta [tcc__r0]
  ldy #1
  lda.l loom_pvs_pass_sleeping
  sta [tcc__r0],y
  ldy #2
  lda.l loom_pvs_pass_stepped
  sta [tcc__r0],y
  rep #$20
  plp
  rtl

; ---------------------------------------------------------------------------
; The contact query (PERF-004): combat's scan for the first live, awake actor
; from `start` whose hit box (contact_damage, hit_width and hit_height all
; nonzero) covers the player's box, other than the player's own shot type.
; r0 <- its index, or $ffff. LoomActorType: hit_x 24, hit_y 26, hit_width
; 28, hit_height 30, contact_damage 37.
;
; The player's box comes from loom_movement_state and its scene (none while
; no scene is active: nothing touches).
; loom_u16 loom_pvs_combat_touch(loom_u16 start 5,s; loom_u16 count 7,s;
;   const LoomActorType *shot 9,s / 11,s)
loom_pvs_combat_touch:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_actor_idx
  lda 7,s
  sta.l loom_pvs_pass_count
  lda 9,s
  sta.l loom_pvs_combat_shot
  lda 11,s
  sta.l loom_pvs_combat_shot+2
  lda.l loom_movement_state+36
  sta.b tcc__r0
  ora.l loom_movement_state+38
  bne +
  brl loom_pvs_touch_none
+ lda.l loom_movement_state+38
  sta.b tcc__r0h                        ; the scene
  ldy #8
  lda.l loom_movement_state+0
  clc
  adc [tcc__r0],y                       ; + collider_x
  sta.l loom_pvs_pass_left
  ldy #12
  clc
  adc [tcc__r0],y                       ; + collider_width
  dec a
  sta.l loom_pvs_pass_right
  ldy #10
  lda.l loom_movement_state+2
  clc
  adc [tcc__r0],y                       ; + collider_y
  sta.l loom_pvs_pass_top
  ldy #14
  clc
  adc [tcc__r0],y                       ; + collider_height
  dec a
  sta.l loom_pvs_pass_bottom
  ; The pool's arrays stay in the direct page for the loop: alive r2,
  ; awake r9, type_ptr r10, x f2, y f3 (this routine owns them all).
  lda.l loom_pvs_actor_arr_alive
  sta.b tcc__r2
  lda.l loom_pvs_actor_arr_alive+2
  sta.b tcc__r2h
  lda.l loom_pvs_actor_arr_awake
  sta.b tcc__r9
  lda.l loom_pvs_actor_arr_awake+2
  sta.b tcc__r9h
  lda.l loom_pvs_actor_arr_type_ptr
  sta.b tcc__r10
  lda.l loom_pvs_actor_arr_type_ptr+2
  sta.b tcc__r10h
  lda.l loom_pvs_actor_arr_x
  sta.b tcc__f2
  lda.l loom_pvs_actor_arr_x+2
  sta.b tcc__f2h
  lda.l loom_pvs_actor_arr_y
  sta.b tcc__f3
  lda.l loom_pvs_actor_arr_y+2
  sta.b tcc__f3h
loom_pvs_touch_loop:
  lda.l loom_pvs_actor_idx
  cmp.l loom_pvs_pass_count
  bcc +
  brl loom_pvs_touch_none
+ tay
  lda [tcc__r2],y
  and #$00ff                            ; alive
  bne +
  brl loom_pvs_touch_next
+ lda [tcc__r9],y
  and #$00ff                            ; awake
  bne +
  brl loom_pvs_touch_next
+ tya
  asl a
  asl a
  tay
  lda [tcc__r10],y
  sta.b tcc__r3
  iny
  iny
  lda [tcc__r10],y
  sta.b tcc__r3h                        ; the type
  ldy #37
  lda [tcc__r3],y
  and #$00ff                            ; contact_damage
  bne +
  brl loom_pvs_touch_next
+ ldy #28
  lda [tcc__r3],y                       ; hit_width
  bne +
  brl loom_pvs_touch_next
+ ldy #30
  lda [tcc__r3],y                       ; hit_height
  bne +
  brl loom_pvs_touch_next
+ lda.l loom_pvs_actor_idx
  asl a
  tay
  lda [tcc__f2],y
  sta.b tcc__r1
  lda [tcc__f3],y
  sta.b tcc__r1h
  ldy #24
  lda.b tcc__r1
  clc
  adc [tcc__r3],y
  sta.b tcc__r4                         ; left
  ldy #28
  clc
  adc [tcc__r3],y
  dec a
  sta.b tcc__r4h                        ; right
  ldy #26
  lda.b tcc__r1h
  clc
  adc [tcc__r3],y
  sta.b tcc__r5                         ; top
  ldy #30
  clc
  adc [tcc__r3],y
  dec a
  sta.b tcc__r5h                        ; bottom
  lda.l loom_pvs_pass_left
  LOOM_SIGNED_CMP tcc__r4h
  beq +
  bmi +
  brl loom_pvs_touch_next               ; player_left > right
+ lda.b tcc__r4
  LOOM_SIGNED_CMP_L loom_pvs_pass_right
  beq +
  bmi +
  brl loom_pvs_touch_next               ; left > player_right
+ lda.l loom_pvs_pass_top
  LOOM_SIGNED_CMP tcc__r5h
  beq +
  bmi +
  brl loom_pvs_touch_next               ; player_top > bottom
+ lda.b tcc__r5
  LOOM_SIGNED_CMP_L loom_pvs_pass_bottom
  beq +
  bmi +
  brl loom_pvs_touch_next               ; top > player_bottom
+ lda.b tcc__r3
  cmp.l loom_pvs_combat_shot
  bne loom_pvs_touch_hit
  lda.b tcc__r3h
  cmp.l loom_pvs_combat_shot+2
  bne loom_pvs_touch_hit
  brl loom_pvs_touch_next               ; the player's own shot
loom_pvs_touch_hit:
  lda.l loom_pvs_actor_idx
  sta.b tcc__r0
  plp
  rtl
loom_pvs_touch_next:
  lda.l loom_pvs_actor_idx
  inc a
  sta.l loom_pvs_actor_idx
  brl loom_pvs_touch_loop
loom_pvs_touch_none:
  lda #$ffff
  sta.b tcc__r0
  plp
  rtl

; ---------------------------------------------------------------------------
; The camera's tick (PERF-004): loom_camera_apply without the snap, reading
; loom_camera_state and the scene by offset and Mode 1's tables through the
; binding above. LoomCameraState: camera_x 0, camera_y 2, auto_fraction_x
; 6, auto_fraction_y 8, facing_x 10, facing_y 11, second_x 14, second_y 16,
; second_present 18, scene 20, min_x 24, min_y 26, max_x 28, max_y 30 (32
; bytes). LoomCameraScene: viewport_anchor_x 0, viewport_anchor_y 2,
; target_slot 4, region_count 5, settings 6, regions 16, target_mode 20 (24
; bytes: a struct holding a pointer is padded to a multiple of four). LoomCameraSettings: follow 0, axis_lock 1, dead_zone_width 2,
; dead_zone_height 3, look_ahead 4, smoothing 5, auto_scroll_x 6,
; auto_scroll_y 8 (10 bytes). LoomCameraRegion: x 0, y 2, width 4, height
; 6, settings 8 (18 bytes). All asserted in camera.c.
;
; An operand like loom_camera_state+24 through a macro argument or a
; .DEFINE assembles with bank 0 (WLA-DX resolves the bank of a computed
; long address only when it is written out directly), so the compares
; against the bounds below are spelled out.

; loom_u16 loom_pvs_camera_update(void): a LoomStatus in r0.
loom_pvs_camera_update:
  php
  rep #$30
  lda.l loom_camera_state+20
  sta.b tcc__r9
  lda.l loom_camera_state+22
  sta.b tcc__r9h                        ; the scene
  ; the target: the sprite behind the target slot
  ldy #4
  lda [tcc__r9],y
  and #$00ff
  tay
  lda.l loom_pvs_mode1_slot_index
  sta.b tcc__r0
  lda.l loom_pvs_mode1_slot_index+2
  sta.b tcc__r0h
  lda [tcc__r0],y
  and #$00ff
  cmp #$00ff
  bne +
  lda #4                                ; LOOM_STATUS_INVALID_HANDLE
  sta.b tcc__r0
  plp
  rtl
+ asl a
  tay
  lda.l loom_pvs_mode1_world_x
  sta.b tcc__r0
  lda.l loom_pvs_mode1_world_x+2
  sta.b tcc__r0h
  lda [tcc__r0],y
  sta.l loom_pvs_cam_target_x
  lda.l loom_pvs_mode1_world_y
  sta.b tcc__r0
  lda.l loom_pvs_mode1_world_y+2
  sta.b tcc__r0h
  lda [tcc__r0],y
  sta.l loom_pvs_cam_target_y
  ; a midpoint scene with a second player frames the middle
  ldy #20
  lda [tcc__r9],y
  and #$00ff
  cmp #1                                ; LOOM_CAMERA_TARGET_MIDPOINT
  bne +
  lda.l loom_camera_state+18
  and #$00ff
  beq +
  lda.l loom_pvs_cam_target_x
  clc
  adc.l loom_camera_state+14
  jsr loom_pvs_camera_half
  sta.l loom_pvs_cam_target_x
  lda.l loom_pvs_cam_target_y
  clc
  adc.l loom_camera_state+16
  jsr loom_pvs_camera_half
  sta.l loom_pvs_cam_target_y
+ ; the settings: the first region holding the target, else the scene's
  lda.b tcc__r9
  clc
  adc #6
  sta.b tcc__r10
  lda.b tcc__r9h
  sta.b tcc__r10h
  ldy #5
  lda [tcc__r9],y
  and #$00ff
  beq loom_pvs_cam_settings_done
  sta.l loom_pvs_cam_regions_left
  ldy #16
  lda [tcc__r9],y
  sta.b tcc__r2
  ldy #18
  lda [tcc__r9],y
  sta.b tcc__r2h                        ; the regions
loom_pvs_cam_region:
  ldy #0
  lda.l loom_pvs_cam_target_x
  LOOM_SIGNED_CMP_IND tcc__r2
  bmi loom_pvs_cam_region_next
  ldy #2
  lda.l loom_pvs_cam_target_y
  LOOM_SIGNED_CMP_IND tcc__r2
  bmi loom_pvs_cam_region_next
  ldy #0
  lda.l loom_pvs_cam_target_x
  sec
  sbc [tcc__r2],y
  ldy #4
  cmp [tcc__r2],y
  bcs loom_pvs_cam_region_next
  ldy #2
  lda.l loom_pvs_cam_target_y
  sec
  sbc [tcc__r2],y
  ldy #6
  cmp [tcc__r2],y
  bcs loom_pvs_cam_region_next
  lda.b tcc__r2
  clc
  adc #8
  sta.b tcc__r10
  lda.b tcc__r2h
  sta.b tcc__r10h
  bra loom_pvs_cam_settings_done
loom_pvs_cam_region_next:
  lda.b tcc__r2
  clc
  adc #18
  sta.b tcc__r2
  lda.l loom_pvs_cam_regions_left
  dec a
  sta.l loom_pvs_cam_regions_left
  bne loom_pvs_cam_region
loom_pvs_cam_settings_done:

  ; X: a room no wider than the view, a locked axis, an auto-scroll, or the
  ; follow rule on the focus (the target plus the look-ahead).
  lda.l loom_camera_state+24
  cmp.l loom_camera_state+28
  bne +
  sta.l loom_pvs_cam_goal_x
  brl loom_pvs_cam_y
+ ldy #1
  lda [tcc__r10],y
  and #$0001                            ; LOOM_CAMERA_LOCK_X
  beq +
  lda.l loom_camera_state+0
  sta.l loom_pvs_cam_goal_x
  brl loom_pvs_cam_y
+ ldy #6
  lda [tcc__r10],y                      ; auto_scroll_x
  beq +
  sta.b tcc__r0
  lda.l loom_camera_state+6
  sta.l loom_pvs_cam_frac
  lda.b tcc__r0
  jsr loom_pvs_camera_auto
  clc
  adc.l loom_camera_state+0
  sta.l loom_pvs_cam_goal_x
  lda.l loom_pvs_cam_frac
  sta.l loom_camera_state+6
  brl loom_pvs_cam_y
+ lda.l loom_pvs_cam_target_x
  sta.l loom_pvs_cam_focus
  sta.l loom_pvs_cam_axis_target
  ldy #4
  lda [tcc__r10],y
  and #$00ff                            ; look_ahead
  beq +
  sta.l loom_pvs_cam_tmp
  lda.l loom_camera_state+10
  and #$00ff                            ; facing_x
  beq +
  bit #$0080                            ; the sign: stored as the caller gave it
  beq ++
  lda.l loom_pvs_cam_focus
  sec
  sbc.l loom_pvs_cam_tmp
  sta.l loom_pvs_cam_focus
  bra +
++ lda.l loom_pvs_cam_focus
  clc
  adc.l loom_pvs_cam_tmp
  sta.l loom_pvs_cam_focus
+ lda.l loom_camera_state+0
  sta.l loom_pvs_cam_axis_camera
  ldy #0
  lda [tcc__r9],y
  sta.l loom_pvs_cam_axis_anchor
  lda.l loom_camera_state+24
  sta.l loom_pvs_cam_axis_origin
  lda #256
  sta.l loom_pvs_cam_axis_page
  ldy #2
  lda [tcc__r10],y
  and #$00ff                            ; dead_zone_width
  sta.l loom_pvs_cam_axis_dz
  jsr loom_pvs_camera_axis_goal
  sta.l loom_pvs_cam_goal_x

loom_pvs_cam_y:
  lda.l loom_camera_state+26
  cmp.l loom_camera_state+30
  bne +
  sta.l loom_pvs_cam_goal_y
  brl loom_pvs_cam_clamp
+ ldy #1
  lda [tcc__r10],y
  and #$0002                            ; LOOM_CAMERA_LOCK_Y
  beq +
  lda.l loom_camera_state+2
  sta.l loom_pvs_cam_goal_y
  brl loom_pvs_cam_clamp
+ ldy #8
  lda [tcc__r10],y                      ; auto_scroll_y
  beq +
  sta.b tcc__r0
  lda.l loom_camera_state+8
  sta.l loom_pvs_cam_frac
  lda.b tcc__r0
  jsr loom_pvs_camera_auto
  clc
  adc.l loom_camera_state+2
  sta.l loom_pvs_cam_goal_y
  lda.l loom_pvs_cam_frac
  sta.l loom_camera_state+8
  brl loom_pvs_cam_clamp
+ lda.l loom_pvs_cam_target_y
  sta.l loom_pvs_cam_focus
  sta.l loom_pvs_cam_axis_target
  ldy #4
  lda [tcc__r10],y
  and #$00ff
  beq +
  sta.l loom_pvs_cam_tmp
  lda.l loom_camera_state+11
  and #$00ff                            ; facing_y
  beq +
  bit #$0080                            ; the sign: stored as the caller gave it
  beq ++
  lda.l loom_pvs_cam_focus
  sec
  sbc.l loom_pvs_cam_tmp
  sta.l loom_pvs_cam_focus
  bra +
++ lda.l loom_pvs_cam_focus
  clc
  adc.l loom_pvs_cam_tmp
  sta.l loom_pvs_cam_focus
+ lda.l loom_camera_state+2
  sta.l loom_pvs_cam_axis_camera
  ldy #2
  lda [tcc__r9],y
  sta.l loom_pvs_cam_axis_anchor
  lda.l loom_camera_state+26
  sta.l loom_pvs_cam_axis_origin
  lda #224
  sta.l loom_pvs_cam_axis_page
  ldy #3
  lda [tcc__r10],y
  and #$00ff                            ; dead_zone_height
  sta.l loom_pvs_cam_axis_dz
  jsr loom_pvs_camera_axis_goal
  sta.l loom_pvs_cam_goal_y

loom_pvs_cam_clamp:
  ; the goal inside the scene's bounds, then eased when the settings smooth
  lda.l loom_pvs_cam_goal_x
  sec
  sbc.l loom_camera_state+24
  bvc +
  eor #$8000
+   bpl +
  lda.l loom_camera_state+24
  sta.l loom_pvs_cam_goal_x
  bra ++
+ lda.l loom_pvs_cam_goal_x
  sec
  sbc.l loom_camera_state+28
  bvc +
  eor #$8000
+   beq ++
  bmi ++
  lda.l loom_camera_state+28
  sta.l loom_pvs_cam_goal_x
++ lda.l loom_pvs_cam_goal_y
  sec
  sbc.l loom_camera_state+26
  bvc +
  eor #$8000
+   bpl +
  lda.l loom_camera_state+26
  sta.l loom_pvs_cam_goal_y
  bra ++
+ lda.l loom_pvs_cam_goal_y
  sec
  sbc.l loom_camera_state+30
  bvc +
  eor #$8000
+   beq ++
  bmi ++
  lda.l loom_camera_state+30
  sta.l loom_pvs_cam_goal_y
++ ldy #5
  lda [tcc__r10],y
  and #$00ff                            ; smoothing
  beq loom_pvs_cam_set
  lda.l loom_camera_state+0
  sta.l loom_pvs_cam_tmp
  lda.l loom_pvs_cam_goal_x
  jsr loom_pvs_camera_approach
  sta.l loom_pvs_cam_goal_x
  lda.l loom_camera_state+2
  sta.l loom_pvs_cam_tmp
  lda.l loom_pvs_cam_goal_y
  jsr loom_pvs_camera_approach
  sta.l loom_pvs_cam_goal_y
loom_pvs_cam_set:
  ; loom_mode1_set_camera: the room's clamp, then the store when it moved
  lda.l loom_pvs_cam_goal_x
  LOOM_SIGNED_CMP_L loom_pvs_mode1_cmin_x
  bpl +
  lda.l loom_pvs_mode1_cmin_x
  sta.l loom_pvs_cam_goal_x
  bra ++
+ lda.l loom_pvs_cam_goal_x
  LOOM_SIGNED_CMP_L loom_pvs_mode1_cmax_x
  beq ++
  bmi ++
  lda.l loom_pvs_mode1_cmax_x
  sta.l loom_pvs_cam_goal_x
++ lda.l loom_pvs_cam_goal_y
  LOOM_SIGNED_CMP_L loom_pvs_mode1_cmin_y
  bpl +
  lda.l loom_pvs_mode1_cmin_y
  sta.l loom_pvs_cam_goal_y
  bra ++
+ lda.l loom_pvs_cam_goal_y
  LOOM_SIGNED_CMP_L loom_pvs_mode1_cmax_y
  beq ++
  bmi ++
  lda.l loom_pvs_mode1_cmax_y
  sta.l loom_pvs_cam_goal_y
++ lda.l loom_pvs_mode1_camera
  sta.b tcc__r0
  lda.l loom_pvs_mode1_camera+2
  sta.b tcc__r0h
  ldy #0
  lda.l loom_pvs_cam_goal_x
  cmp [tcc__r0],y
  bne +
  ldy #2
  lda.l loom_pvs_cam_goal_y
  cmp [tcc__r0],y
  beq ++
+ ldy #0
  lda.l loom_pvs_cam_goal_x
  sta [tcc__r0],y
  ldy #2
  lda.l loom_pvs_cam_goal_y
  sta [tcc__r0],y
  sep #$20
  lda.l loom_mode1_debug_epoch
  inc a
  sta.l loom_mode1_debug_epoch
  rep #$20
++ ldy #0
  lda [tcc__r0],y
  sta.l loom_camera_state+0
  ldy #2
  lda [tcc__r0],y
  sta.l loom_camera_state+2
  lda #0
  sta.b tcc__r0                         ; LOOM_STATUS_OK
  plp
  rtl

; A <- A / 2 toward zero, as the C's signed divide.
loom_pvs_camera_half:
  bpl +
  inc a
+ cmp #$8000
  ror a
  rts

; One axis of the follow rule for a tick (no snap): the goal in A from
; loom_pvs_cam_focus, _axis_camera, _axis_anchor, _axis_origin, _axis_page,
; _axis_dz and _axis_target, with the settings in r10.
loom_pvs_camera_axis_goal:
  ldy #0
  lda [tcc__r10],y
  and #$00ff                            ; follow
  cmp #1
  beq loom_pvs_cam_dead_zone
  cmp #2
  beq loom_pvs_cam_screens
  lda.l loom_pvs_cam_focus
  sec
  sbc.l loom_pvs_cam_axis_anchor        ; center: focus - anchor
  rts
loom_pvs_cam_dead_zone:
  lda.l loom_pvs_cam_axis_dz
  lsr a
  sta.l loom_pvs_cam_tmp
  lda.l loom_pvs_cam_axis_camera
  clc
  adc.l loom_pvs_cam_axis_anchor
  sec
  sbc.l loom_pvs_cam_tmp
  sta.l loom_pvs_cam_low
  clc
  adc.l loom_pvs_cam_axis_dz
  sta.l loom_pvs_cam_high
  lda.l loom_pvs_cam_focus
  LOOM_SIGNED_CMP_L loom_pvs_cam_low
  bpl +
  lda.l loom_pvs_cam_low
  sec
  sbc.l loom_pvs_cam_focus
  sta.l loom_pvs_cam_tmp
  lda.l loom_pvs_cam_axis_camera
  sec
  sbc.l loom_pvs_cam_tmp                ; camera - (low - focus)
  rts
+ lda.l loom_pvs_cam_high
  LOOM_SIGNED_CMP_L loom_pvs_cam_focus
  bpl +
  lda.l loom_pvs_cam_focus
  sec
  sbc.l loom_pvs_cam_high
  clc
  adc.l loom_pvs_cam_axis_camera        ; camera + (focus - high)
  rts
+ lda.l loom_pvs_cam_axis_camera
  rts
loom_pvs_cam_screens:
  lda.l loom_pvs_cam_axis_target
  sec
  sbc.l loom_pvs_cam_axis_origin
  bpl +
  lda.l loom_pvs_cam_axis_origin
  rts
+ sta.b tcc__r0
  lda.l loom_pvs_cam_axis_page
  sta.b tcc__r0h
  jsr loom_pvs_player_udiv              ; offset / page
  sta.b tcc__r0
  lda.l loom_pvs_cam_axis_page
  sta.b tcc__r0h
  jsr loom_pvs_player_umul              ; * page
  clc
  adc.l loom_pvs_cam_axis_origin
  rts

; A (goal) eased from loom_pvs_cam_tmp (current): an eighth of the gap, at
; least a pixel. A <- the new position.
loom_pvs_camera_approach:
  sec
  sbc.l loom_pvs_cam_tmp
  bne +
  lda.l loom_pvs_cam_tmp
  rts
+ bmi ++
  lsr a
  lsr a
  lsr a
  bne +
  lda #1
+ clc
  adc.l loom_pvs_cam_tmp
  rts
++ eor #$ffff
  inc a
  lsr a
  lsr a
  lsr a
  bne +
  lda #1
+ sta.b tcc__r0h
  lda.l loom_pvs_cam_tmp
  sec
  sbc.b tcc__r0h
  rts

; An auto-scroll step: A the signed 8.8 speed, loom_pvs_cam_frac the carried
; fraction (kept in 0..255, updated). A <- the whole pixels this tick.
loom_pvs_camera_auto:
  bmi +
  sta.l loom_pvs_cam_tmp
  and #$00ff
  sta.l loom_pvs_cam_part
  lda.l loom_pvs_cam_tmp
  xba
  and #$00ff
  sta.l loom_pvs_cam_whole
  bra ++
+ eor #$ffff
  inc a
  sta.l loom_pvs_cam_tmp
  and #$00ff
  eor #$ffff
  inc a
  sta.l loom_pvs_cam_part               ; -(m & 255)
  lda.l loom_pvs_cam_tmp
  xba
  and #$00ff
  eor #$ffff
  inc a
  sta.l loom_pvs_cam_whole              ; -(m >> 8)
++ lda.l loom_pvs_cam_frac
  clc
  adc.l loom_pvs_cam_part
  bpl +
  clc
  adc #256
  sta.l loom_pvs_cam_frac
  lda.l loom_pvs_cam_whole
  dec a
  rts
+ cmp #256
  bcc ++
  sec
  sbc #256
  sta.l loom_pvs_cam_frac
  lda.l loom_pvs_cam_whole
  inc a
  rts
++ sta.l loom_pvs_cam_frac
  lda.l loom_pvs_cam_whole
  rts

; ---------------------------------------------------------------------------
; The animation pass (PERF-004): loom_animation_update's per-player tick --
; a playing clip's elapsed ticks count up, and a player whose frame has
; run its duration goes on the due list for C to advance (the next frame,
; the loop, the landing clip's return, the pose). LoomAnimationFrame:
; duration_ticks at 6, 12 bytes; asserted in animation.c.
;
; void loom_pvs_animation_bind(loom_u8 *playing 5,s; loom_u16 *elapsed 9,s;
;                              loom_u8 *frame_index 13,s;
;                              const LoomAnimationFrame **frames 17,s)
loom_pvs_animation_bind:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_anim_playing
  lda 7,s
  sta.l loom_pvs_anim_playing+2
  lda 9,s
  sta.l loom_pvs_anim_elapsed
  lda 11,s
  sta.l loom_pvs_anim_elapsed+2
  lda 13,s
  sta.l loom_pvs_anim_frame_index
  lda 15,s
  sta.l loom_pvs_anim_frame_index+2
  lda 17,s
  sta.l loom_pvs_anim_frames
  lda 19,s
  sta.l loom_pvs_anim_frames+2
  plp
  rtl

; loom_u16 loom_pvs_animation_pass(loom_u16 count 5,s; loom_u8 *due 7,s):
; r0 <- how many players are due.
loom_pvs_animation_pass:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_pass_count
  lda 7,s
  sta.l loom_pvs_pass_slow
  lda 9,s
  sta.l loom_pvs_pass_slow+2
  lda #0
  sta.l loom_pvs_pass_slow_count
  sta.l loom_pvs_actor_idx
loom_pvs_anim_loop:
  lda.l loom_pvs_actor_idx
  cmp.l loom_pvs_pass_count
  bcc +
  brl loom_pvs_anim_done
+ asl a
  sta.l loom_pvs_actor_idx2
  asl a
  sta.l loom_pvs_actor_idx4
  LOOM_ACTOR_LOAD_BYTE loom_pvs_anim_playing
  bne +
  brl loom_pvs_anim_next
+ LOOM_ACTOR_LOAD_PTR loom_pvs_anim_frames tcc__r3 tcc__r3h
  LOOM_ACTOR_LOAD_BYTE loom_pvs_anim_frame_index
  sta.b tcc__r4
  asl a
  adc.b tcc__r4
  asl a
  asl a                                 ; frame_index * 12
  clc
  adc.b tcc__r3
  sta.b tcc__r3
  LOOM_ACTOR_LOAD_WORD loom_pvs_anim_elapsed tcc__r5
  lda.b tcc__r5
  inc a
  sta.b tcc__r5
  LOOM_ACTOR_STORE_WORD loom_pvs_anim_elapsed tcc__r5
  ldy #6
  lda.b tcc__r5
  cmp [tcc__r3],y                       ; elapsed < duration_ticks: not yet
  bcs +
  brl loom_pvs_anim_next
+ lda.l loom_pvs_pass_slow
  sta.b tcc__r0
  lda.l loom_pvs_pass_slow+2
  sta.b tcc__r0h
  lda.l loom_pvs_pass_slow_count
  tay
  inc a
  sta.l loom_pvs_pass_slow_count
  lda.l loom_pvs_actor_idx
  sep #$20
  sta [tcc__r0],y
  rep #$20
loom_pvs_anim_next:
  lda.l loom_pvs_actor_idx
  inc a
  sta.l loom_pvs_actor_idx
  brl loom_pvs_anim_loop
loom_pvs_anim_done:
  lda.l loom_pvs_pass_slow_count
  sta.b tcc__r0
  plp
  rtl

.ENDS
