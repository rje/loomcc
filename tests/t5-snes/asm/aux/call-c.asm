.include "hdr.asm"
.accu 16
.index 16
.16bit
; i16 asm_sum_squares(i16 a, i16 b): a at 4,s, b at 6,s (816-tcc ABI).
.SECTION "asm_sum_squares" SUPERFREE
asm_sum_squares:
  rep #$30
  lda 4,s
  pha
  jsl c_square
  pla
  lda.b tcc__r0
  pha                 ; keep c_square(a)
  lda 8,s             ; b (two more bytes on the stack now)
  pha
  jsl c_square
  pla
  pla
  clc
  adc.b tcc__r0
  sta.b tcc__r0
  rtl
.ENDS

; void asm_touch_twice(Probe *p, i16 dx): p at 4,s (4 bytes), dx at 8,s.
.SECTION "asm_touch_twice" SUPERFREE
asm_touch_twice:
  rep #$30
  lda 8,s             ; dx
  pha
  lda 8,s             ; p bank word (4,s + 2 moved by the push)
  pha
  lda 8,s             ; p low word
  pha
  jsl c_touch
  pla
  pla
  pla
  lda 8,s
  pha
  lda 8,s
  pha
  lda 8,s
  pha
  jsl c_touch
  pla
  pla
  pla
  rtl
.ENDS
