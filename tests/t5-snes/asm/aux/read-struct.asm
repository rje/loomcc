.include "hdr.asm"
.accu 16
.index 16
.16bit

; u16 asm_body_sum(struct Body *b), 816-tcc ABI: b is a 4-byte pointer at 4,s.
; struct Body { u8 flags; u8 *sprite; i16 x; i16 y; u8 frame; }
; 816-tcc layout: flags 0, sprite 4 (pointer, 4-aligned), x 8, y 10, frame 12.
.SECTION "asm_body_sum" SUPERFREE
asm_body_sum:
  rep #$30
  lda 4,s
  sta.b tcc__r1
  lda 6,s
  sta.b tcc__r1h
  ldy.w #0
  lda [tcc__r1],y
  and.w #$00ff
  sta.b tcc__r0
  ldy.w #8
  lda [tcc__r1],y
  clc
  adc.b tcc__r0
  sta.b tcc__r0
  ldy.w #10
  lda [tcc__r1],y
  clc
  adc.b tcc__r0
  sta.b tcc__r0
  ldy.w #12
  lda [tcc__r1],y
  and.w #$00ff
  clc
  adc.b tcc__r0
  sta.b tcc__r0
  ; sprite pointer at offset 4 (low word) and 6 (bank)
  ldy.w #4
  lda [tcc__r1],y
  sta.b tcc__r2
  ldy.w #6
  lda [tcc__r1],y
  sta.b tcc__r2h
  lda [tcc__r2]
  and.w #$00ff
  clc
  adc.b tcc__r0
  sta.b tcc__r0
  rtl
.ENDS
