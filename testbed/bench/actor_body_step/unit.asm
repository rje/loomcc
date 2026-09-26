.include "hdr.asm"

.accu 16
.index 16
.16bit

; actor_body_step: loom_pvs_actor_body from Loom 13d8987
; runtime/backends/pvsneslib/src/body.asm, verbatim, with the helpers it
; uses (loom_pvs_actor_pointers/_s8/_advance, loom_pvs_body_cell/_grid), the
; signed compare macros and its scratch RAM. Two changes of placement only:
; the RAM section is in bank $7e slot 2 (13d8987 had it in bank 0 slot 1;
; 925fc3a moved it to $7e, and every access is a long one either way), and
; `.BASE LOOM_ROM_BASE` is written `.BASE $80` (the harness's FastROM). The
; tile probes it calls with jsl (loom_pvs_body_probe_x, _scan_floor,
; _scan_ceiling) are provided by driver.c for both variants.

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
.ENDS
.BASE $80

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
  plp
  rtl

.ENDS
