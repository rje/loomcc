.include "hdr.asm"

.accu 16
.index 16
.16bit

; solid_actor_query: the pool's solid-actor queries from Loom ea4e3af
; runtime/backends/pvsneslib/src/body.asm -- loom_pvs_solid_next,
; loom_pvs_solid_floor and loom_pvs_solid_wall, verbatim (unchanged at
; HEAD), with the macros and scratch RAM they use. In Loom they are jsr
; routines the assembly bodies call with the box in loom_pvs_sq_*; the two
; 816-tcc ABI entries at the top of the code section are bench glue (not
; Loom code) standing where the C shims the bodies used to call stood:
; they store the box and jsr, as body.asm's loom_pvs_probe_actor_floor and
; _wall do. The pool and Mode 1 bindings (loom_pvs_actor_arr_*,
; loom_pvs_solid_slots, loom_pvs_solid_visible) are defined and bound by
; driver.c, as loom_pvs_actor_bind_pass and loom_pvs_solid_bind do at
; activation.

.BASE $00
.RAMSECTION "loom.pvs.actor.body.state" BANK $7e SLOT 2
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

; Bench glue: loom_u16 loom_movement_actor_floor_probe(loom_s16 left 5,s;
;   loom_s16 right 7,s; loom_s16 top 9,s; loom_s16 bottom 11,s)
loom_movement_actor_floor_probe:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_sq_left
  lda 7,s
  sta.l loom_pvs_sq_right
  lda 9,s
  sta.l loom_pvs_sq_top
  lda 11,s
  sta.l loom_pvs_sq_bottom
  jsr loom_pvs_solid_floor
  sta.b tcc__r0
  plp
  rtl

; Bench glue: loom_u16 loom_movement_actor_block_probe(loom_s16 left 5,s;
;   loom_s16 top 7,s; loom_s16 right 9,s; loom_s16 bottom 11,s)
loom_movement_actor_block_probe:
  php
  rep #$30
  lda 5,s
  sta.l loom_pvs_sq_left
  lda 7,s
  sta.l loom_pvs_sq_top
  lda 9,s
  sta.l loom_pvs_sq_right
  lda 11,s
  sta.l loom_pvs_sq_bottom
  jsr loom_pvs_solid_wall
  sta.b tcc__r0
  plp
  rtl

; The pool's solid-actor queries (loom_actor_floor_below with no self, and
; loom_actor_blocks_box outside a C resolve), over the bound arrays with
; only tcc__r0/r0h touched, so the bodies' direct page needs no saving
; around them. The box is loom_pvs_sq_left/right/top/bottom (inclusive).


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

.ENDS
