.include "hdr.asm"

.accu 16
.index 16
.16bit

; camera_follow: loom_pvs_camera_update from Loom 925fc3a
; runtime/backends/pvsneslib/src/body.asm, verbatim, with its local helpers
; (loom_pvs_camera_half/_axis_goal/_approach/_auto, and the player tick's
; loom_pvs_player_umul/_udiv it borrows for the screens rule), the two signed
; compare macros it uses and its own scratch RAM. Mode 1's bindings
; (loom_pvs_mode1_*, set by loom_pvs_mode1_bind at every scene activation in
; Loom) are defined and bound by driver.c, as is loom_camera_state.
; `.BASE LOOM_ROM_BASE` is written `.BASE $80` (the harness's LoROM FastROM).

.BASE $00
.RAMSECTION "loom.pvs.actor.body.state" BANK $7e SLOT 2
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
.ENDS
.BASE $80

.SECTION "loom.pvs.body.code" SUPERFREE KEEP

; Signed 16-bit compare, A against a word in RAM by long address: leaves N
; set when A is less, as the C's signed compare would. Clobbers A.
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
  cmp #1
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
  cmp #1
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

.ENDS
