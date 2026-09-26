.include "hdr.asm"

.accu 16
.index 16
.16bit

; From Loom body.asm (HEAD; loom_pvs_place_sprite and the player tail's
; sprite lookup are as at 5184be9): the RAM loom_pvs_mode1_bind and
; loom_pvs_place_sprite use out of "loom.pvs.actor.body.state", the binding,
; and the placement.

.BASE $00
.RAMSECTION "loom.pvs.actor.body.state" BANK $7e SLOT 2
loom_pvs_mode1_world_x dsb 4
loom_pvs_mode1_world_y dsb 4
loom_pvs_mode1_part_count dsb 4
loom_pvs_mode1_sprite_count dsb 2
loom_pvs_pass_cursor dsb 2
loom_pvs_pass_remaining dsb 2
loom_pvs_mode1_slot_index dsb 4
loom_pvs_mode1_camera dsb 4
loom_pvs_mode1_cmin_x dsb 2
loom_pvs_mode1_cmin_y dsb 2
loom_pvs_mode1_cmax_x dsb 2
loom_pvs_mode1_cmax_y dsb 2
.ENDS
.BASE $80

.SECTION "loom.pvs.body.code" SUPERFREE KEEP

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

; void loom_pvs_player_sprite(loom_u8 slot 5,s; loom_s16 world_x 6,s;
;                             loom_s16 world_y 8,s)
; Benchmark entry, not in Loom: loom_pvs_player_tail ends by placing the
; player's sprite with x in r1 and y in r1h and the slot read from the
; movement scene; this loads those from the arguments and then runs the
; tail's lookup and call verbatim.
loom_pvs_player_sprite:
  php
  rep #$30
  lda 6,s
  sta.b tcc__r1
  lda 8,s
  sta.b tcc__r1h
  lda 5,s
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

.ENDS
