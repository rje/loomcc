.include "hdr.asm"

.accu 16
.index 16
.16bit

; The frame build's layer scroll (PERF-004), loom_mode1_build_frame's scroll
; block for a scene with no drifting layer: BG1 follows the camera and each
; layer scrolls by camera * numerator / denominator, truncated toward zero,
; with the last tick's result reused while the camera stands still.
;
; loom_u16 loom_pvs_mode1_scroll(LoomDisplayState *display 5,s;
;     LoomMode1ScrollCache *cache 9,s; const LoomMode1Layer *layers 13,s;
;     loom_u16 layer_count 17,s; loom_s16 camera_x 19,s;
;     loom_s16 camera_y 21,s)
; r0 <- has_bg2 | has_bg3 << 1, or $ffff for a drifting layer (the C path
; then does the whole block; anything written here it writes again).
;
; LoomDisplayState: bg_scroll_x[4] @0, bg_scroll_y[4] @8.
; LoomMode1ScrollCache: x[4] @0, y[4] @8, camera_x @16, camera_y @18,
; has_bg2 @20, has_bg3 @21, valid @22.
; LoomMode1Layer (10 bytes): background @0, scroll_x_numerator @2,
; scroll_x_denominator @3, scroll_y_numerator @4, scroll_y_denominator @5,
; auto_scroll_x @6, auto_scroll_y @8.
; Scratch: r1 display, r2 cache, r3 the layer record, r4 the count, r5 the
; camera x, r5h the camera y, r9 the flags, r9h the layer index.

.BASE $00
.RAMSECTION "loom.pvs.mode1.scroll.state" BANK $7e SLOT 2
loom_pvs_scroll_value dsb 2
loom_pvs_scroll_num dsb 2
loom_pvs_scroll_den dsb 2
loom_pvs_scroll_neg dsb 2
loom_pvs_scroll_rem dsb 2
loom_pvs_scroll_bg dsb 2
.ENDS
.BASE $80

.SECTION "loom.pvs.mode1.scroll.code" SUPERFREE KEEP

loom_pvs_mode1_scroll:
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
  sta.b tcc__r3
  lda 15,s
  sta.b tcc__r3h
  lda 17,s
  sta.b tcc__r4
  lda 19,s
  sta.b tcc__r5
  lda 21,s
  sta.b tcc__r5h
  ; A still camera reuses the last tick's scroll.
  ldy #22
  lda [tcc__r2],y
  and #$00ff
  beq loom_pvs_scroll_compute
  ldy #16
  lda [tcc__r2],y
  cmp.b tcc__r5
  bne loom_pvs_scroll_compute
  ldy #18
  lda [tcc__r2],y
  cmp.b tcc__r5h
  bne loom_pvs_scroll_compute
  ldy #14
- lda [tcc__r2],y
  sta [tcc__r1],y
  dey
  dey
  bpl -
  ldy #20
  lda [tcc__r2],y
  and #$0001
  sta.b tcc__r0
  ldy #21
  lda [tcc__r2],y
  and #$0001
  asl a
  ora.b tcc__r0
  sta.b tcc__r0
  plp
  rtl

loom_pvs_scroll_compute:
  lda.b tcc__r5
  ldy #0
  sta [tcc__r1],y                       ; bg_scroll_x[0] = camera_x
  lda.b tcc__r5h
  ldy #8
  sta [tcc__r1],y                       ; bg_scroll_y[0] = camera_y
  stz.b tcc__r9
  stz.b tcc__r9h
loom_pvs_scroll_layer:
  lda.b tcc__r9h
  cmp.b tcc__r4
  bcc +
  brl loom_pvs_scroll_cache
+ ; a drifting layer is the C path's
  ldy #6
  lda [tcc__r3],y
  ldy #8
  ora [tcc__r3],y
  beq +
  lda #$ffff
  sta.b tcc__r0
  plp
  rtl
+ lda [tcc__r3]
  and #$00ff
  asl a
  sta.l loom_pvs_scroll_bg              ; background * 2
  ; x
  ldy #2
  lda [tcc__r3],y
  and #$00ff
  sta.l loom_pvs_scroll_num
  ldy #3
  lda [tcc__r3],y
  and #$00ff
  sta.l loom_pvs_scroll_den
  lda.b tcc__r5
  jsr loom_pvs_scroll_scaled
  pha
  lda.l loom_pvs_scroll_bg
  tay
  pla
  sta [tcc__r1],y
  ; y
  ldy #4
  lda [tcc__r3],y
  and #$00ff
  sta.l loom_pvs_scroll_num
  ldy #5
  lda [tcc__r3],y
  and #$00ff
  sta.l loom_pvs_scroll_den
  lda.b tcc__r5h
  jsr loom_pvs_scroll_scaled
  pha
  lda.l loom_pvs_scroll_bg
  clc
  adc #8
  tay
  pla
  sta [tcc__r1],y
  ; BG2 and BG3 are on when a layer drives them
  lda.l loom_pvs_scroll_bg
  cmp #2
  bne +
  lda.b tcc__r9
  ora #1
  sta.b tcc__r9
+ lda.l loom_pvs_scroll_bg
  cmp #4
  bne +
  lda.b tcc__r9
  ora #2
  sta.b tcc__r9
+ lda.b tcc__r3
  clc
  adc #10
  sta.b tcc__r3
  inc.b tcc__r9h
  brl loom_pvs_scroll_layer

loom_pvs_scroll_cache:
  ldy #14
- lda [tcc__r1],y
  sta [tcc__r2],y
  dey
  dey
  bpl -
  lda.b tcc__r5
  ldy #16
  sta [tcc__r2],y
  lda.b tcc__r5h
  ldy #18
  sta [tcc__r2],y
  sep #$20
  lda.b tcc__r9
  and #$01
  ldy #20
  sta [tcc__r2],y
  lda.b tcc__r9
  lsr a
  and #$01
  iny
  sta [tcc__r2],y
  lda #1
  iny
  sta [tcc__r2],y                       ; valid
  rep #$20
  lda.b tcc__r9
  sta.b tcc__r0
  plp
  rtl

; A <- A * loom_pvs_scroll_num / loom_pvs_scroll_den, the magnitude scaled
; and the sign put back (loom_mode1_scaled). Clobbers X, r0, r10.
loom_pvs_scroll_scaled:
  sta.l loom_pvs_scroll_value
  lda.l loom_pvs_scroll_num
  cmp.l loom_pvs_scroll_den
  bne +
  lda.l loom_pvs_scroll_value
  rts
+ lda #0
  sta.l loom_pvs_scroll_neg
  lda.l loom_pvs_scroll_value
  bpl +
  eor #$ffff
  inc a
  pha
  lda #1
  sta.l loom_pvs_scroll_neg
  pla
+ sta.b tcc__r0                         ; the magnitude
  ; * numerator, wrapping as the C's u16 does
  lda.l loom_pvs_scroll_num
  cmp #1
  beq loom_pvs_scroll_divide
  tax
  lda #0
  cpx #0
  beq +                                 ; a numerator of zero holds the layer
- clc
  adc.b tcc__r0
  dex
  bne -
+ sta.b tcc__r0
loom_pvs_scroll_divide:
  ; / denominator: a shift for 1, 2, 4 and 8, else a divide
  lda.l loom_pvs_scroll_den
  cmp #1
  beq loom_pvs_scroll_sign
  cmp #2
  bne +
  lsr.b tcc__r0
  bra loom_pvs_scroll_sign
+ cmp #4
  bne +
  lsr.b tcc__r0
  lsr.b tcc__r0
  bra loom_pvs_scroll_sign
+ cmp #8
  bne +
  lsr.b tcc__r0
  lsr.b tcc__r0
  lsr.b tcc__r0
  bra loom_pvs_scroll_sign
+ ; r0 / den: the quotient shifts into r0 as the dividend shifts out
  sta.b tcc__r10
  lda #0
  sta.l loom_pvs_scroll_rem
  ldx #16
- asl.b tcc__r0
  lda.l loom_pvs_scroll_rem
  rol a
  cmp.b tcc__r10
  bcc +
  sbc.b tcc__r10
  inc.b tcc__r0
+ sta.l loom_pvs_scroll_rem
  dex
  bne -
loom_pvs_scroll_sign:
  lda.l loom_pvs_scroll_neg
  beq +
  lda.b tcc__r0
  eor #$ffff
  inc a
  rts
+ lda.b tcc__r0
  rts

.ENDS
