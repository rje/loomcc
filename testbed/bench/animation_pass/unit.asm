.include "hdr.asm"

.accu 16
.index 16
.16bit

; From Loom body.asm (HEAD, unchanged since 5184be9): the RAM the pass uses
; out of "loom.pvs.actor.body.state", the actor-array macros it uses, and
; the animation pass itself.

.BASE $00
.RAMSECTION "loom.pvs.actor.body.state" BANK $7e SLOT 2
loom_pvs_actor_idx dsb 2
loom_pvs_actor_idx2 dsb 2
loom_pvs_actor_idx4 dsb 2
loom_pvs_pass_count dsb 2
loom_pvs_pass_slow dsb 4
loom_pvs_pass_slow_count dsb 2
loom_pvs_anim_playing dsb 4
loom_pvs_anim_elapsed dsb 4
loom_pvs_anim_frame_index dsb 4
loom_pvs_anim_frames dsb 4
.ENDS
.BASE $80

.SECTION "loom.pvs.body.code" SUPERFREE KEEP

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
