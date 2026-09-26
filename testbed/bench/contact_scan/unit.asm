.include "hdr.asm"

.accu 16
.index 16
.16bit

; contact_scan: loom_pvs_combat_touch from Loom 925fc3a
; runtime/backends/pvsneslib/src/body.asm, verbatim, with the macros and the
; RAM it uses. The pool's array bindings (loom_pvs_actor_arr_*, set by
; loom_pvs_actor_bind / loom_pvs_actor_bind_pass once per activation in
; Loom) are defined and bound by driver.c; the scan's own scratch RAM is
; here.

.BASE $00
.RAMSECTION "loom.pvs.actor.body.state" BANK $7e SLOT 2
loom_pvs_actor_idx dsb 2
loom_pvs_actor_idx2 dsb 2
loom_pvs_actor_idx4 dsb 2
loom_pvs_pass_count dsb 2
loom_pvs_pass_left dsb 2
loom_pvs_pass_top dsb 2
loom_pvs_pass_right dsb 2
loom_pvs_pass_bottom dsb 2
loom_pvs_combat_shot dsb 4
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


; ---------------------------------------------------------------------------
; The contact query (PERF-004): combat's scan for the first live, awake actor
; from `start` whose hit box (contact_damage, hit_width and hit_height all
; nonzero) covers the player's box, other than the player's own shot type.
; r0 <- its index, or $ffff. LoomActorType: hit_x 24, hit_y 26, hit_width
; 28, hit_height 30, contact_damage 37.
;
; loom_u16 loom_pvs_combat_touch(loom_u16 start 5,s; loom_u16 count 7,s;
;   loom_s16 left 9,s; top 11,s; right 13,s; bottom 15,s;
;   const LoomActorType *shot 17,s / 19,s)
loom_pvs_combat_touch:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_actor_idx
  lda 7,s
  sta.l loom_pvs_pass_count
  lda 9,s
  sta.l loom_pvs_pass_left
  lda 11,s
  sta.l loom_pvs_pass_top
  lda 13,s
  sta.l loom_pvs_pass_right
  lda 15,s
  sta.l loom_pvs_pass_bottom
  lda 17,s
  sta.l loom_pvs_combat_shot
  lda 19,s
  sta.l loom_pvs_combat_shot+2
loom_pvs_touch_loop:
  lda.l loom_pvs_actor_idx
  cmp.l loom_pvs_pass_count
  bcc +
  brl loom_pvs_touch_none
+ asl a
  sta.l loom_pvs_actor_idx2
  asl a
  sta.l loom_pvs_actor_idx4
  LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_alive
  bne +
  brl loom_pvs_touch_next
+ LOOM_ACTOR_LOAD_BYTE loom_pvs_actor_arr_awake
  bne +
  brl loom_pvs_touch_next
+ LOOM_ACTOR_LOAD_PTR loom_pvs_actor_arr_type_ptr tcc__r3 tcc__r3h
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
+ LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_x tcc__r1
  LOOM_ACTOR_LOAD_WORD loom_pvs_actor_arr_y tcc__r1h
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

.ENDS
