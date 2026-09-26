.include "hdr.asm"
.accu 16
.index 16
.16bit
; u16 asm_pack(u8 a, u8 b, u16 c, u8 d): a at 4,s, b at 5,s, c at 6,s, d at 8,s.
.SECTION "asm_pack" SUPERFREE
asm_pack:
  rep #$30
  lda 5,s
  and.w #$00ff
  asl a
  asl a
  asl a
  asl a
  sta.b tcc__r0
  lda 4,s
  and.w #$00ff
  clc
  adc.b tcc__r0
  clc
  adc 6,s
  sta.b tcc__r0
  lda 8,s
  and.w #$00ff
  xba
  asl a
  asl a
  asl a
  asl a
  clc
  adc.b tcc__r0
  sta.b tcc__r0
  rtl
.ENDS
