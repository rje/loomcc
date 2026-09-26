.include "hdr.asm"

.accu 16
.index 16
.16bit

.SECTION "loom.pvs.body.code" SUPERFREE KEEP

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

.ENDS
