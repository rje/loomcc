.include "hdr.asm"

.accu 16
.index 16
.16bit

; player_tick: loom_pvs_player_tick from Loom 925fc3a
; runtime/backends/pvsneslib/src/body.asm, verbatim, with every routine it
; reaches (the tight probes loom_pvs_tight_cell/_advance/_probe_x/
; _scan_floor/_scan_ceiling, and loom_pvs_player_ease/_slope_feet/
; _jump_scale/_umul/_udiv), the signed compare macros and the scratch RAM
; they use (the rest of body.asm's RAM section is left out).
; `.BASE LOOM_ROM_BASE` is written `.BASE $80` (the harness's FastROM).

.BASE $00
.RAMSECTION "loom.pvs.actor.body.state" BANK $7e SLOT 2
loom_pvs_actor_intent dsb 2
loom_pvs_actor_grounded dsb 2
loom_pvs_actor_next_sub dsb 2
loom_pvs_actor_delta dsb 2
loom_pvs_actor_edge dsb 2
loom_pvs_actor_wall_bottom dsb 2
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

; The same against a word in RAM by long address.
.MACRO LOOM_SIGNED_CMP_L ARGS operand
  sec
  sbc.l operand
  bvc _skipl\@
  eor #$8000
_skipl\@:
.ENDM



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
  sta.l loom_movement_state+23          ; riding: no solid actors here
  rep #$20
  ldx.b tcc__r5
  lda.b tcc__r4h
  jsr loom_pvs_player_slope_feet        ; the slope under the sensor
  bcc loom_pvs_player_snap
  sta.b tcc__r0
  lda.l loom_pvs_actor_landed
  bmi +
  lda.b tcc__r0
  LOOM_SIGNED_CMP_L loom_pvs_actor_landed
  bpl loom_pvs_player_snap              ; the scan's floor is higher
+ lda.b tcc__r0
  sta.l loom_pvs_actor_landed
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
  plp
  rtl

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


.ENDS
