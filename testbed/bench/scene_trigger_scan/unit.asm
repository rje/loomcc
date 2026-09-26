.include "hdr.asm"

.accu 16
.index 16
.16bit

; The trigger scan's box test for every trigger of the scene at once, in
; assembly because six triggers cost 816-tcc about forty scanlines in C.
; Returns the bit mask of triggers whose rectangle overlaps the player box
; (bit i for trigger i), with the same open-interval rule as scene.c:
; left < x + width, right > x, top < y + height, bottom > y.
;
; loom_u16 loom_pvs_scene_intersect_mask(const LoomSceneBox *player,
;                                        const LoomSceneTrigger *triggers,
;                                        loom_u8 count)
; LoomSceneBox: left @0, top @2, right @4, bottom @6.
; LoomSceneTrigger (24 bytes): x @0, y @2, width @4, height @6.
; Scratch: r1 player, r2 trigger, r3 count, r4 left, r4h top, r5 right,
; r5h bottom, r9 bit, r10 index, r0 mask (the result).

.SECTION "loom.pvs.scene.code" SUPERFREE KEEP

loom_pvs_scene_intersect_mask:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r1
  lda 7,s
  sta.b tcc__r1h
  lda 9,s
  sta.b tcc__r2
  lda 11,s
  sta.b tcc__r2h
  lda 13,s
  and #$00ff
  sta.b tcc__r3
  ldy #0
  lda [tcc__r1],y
  sta.b tcc__r4
  ldy #2
  lda [tcc__r1],y
  sta.b tcc__r4h
  ldy #4
  lda [tcc__r1],y
  sta.b tcc__r5
  ldy #6
  lda [tcc__r1],y
  sta.b tcc__r5h
  stz.b tcc__r0
  lda #1
  sta.b tcc__r9
  stz.b tcc__r10
loom_pvs_scene_intersect_loop:
  lda.b tcc__r10
  cmp.b tcc__r3
  bcs loom_pvs_scene_intersect_done
  ; left < x + width
  ldy #4
  lda [tcc__r2],y
  ldy #0
  clc
  adc [tcc__r2],y
  sec
  sbc.b tcc__r4
  beq loom_pvs_scene_intersect_next
  bmi loom_pvs_scene_intersect_next
  ; right > x
  lda.b tcc__r5
  sec
  sbc [tcc__r2],y
  beq loom_pvs_scene_intersect_next
  bmi loom_pvs_scene_intersect_next
  ; top < y + height
  ldy #6
  lda [tcc__r2],y
  ldy #2
  clc
  adc [tcc__r2],y
  sec
  sbc.b tcc__r4h
  beq loom_pvs_scene_intersect_next
  bmi loom_pvs_scene_intersect_next
  ; bottom > y
  lda.b tcc__r5h
  sec
  sbc [tcc__r2],y
  beq loom_pvs_scene_intersect_next
  bmi loom_pvs_scene_intersect_next
  lda.b tcc__r0
  ora.b tcc__r9
  sta.b tcc__r0
loom_pvs_scene_intersect_next:
  asl.b tcc__r9
  inc.b tcc__r10
  lda.b tcc__r2
  clc
  adc #24
  sta.b tcc__r2
  bra loom_pvs_scene_intersect_loop
loom_pvs_scene_intersect_done:
  plp
  rtl

.ENDS
