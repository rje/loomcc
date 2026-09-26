.include "hdr.asm"
.accu 16
.index 16
.16bit
; u16 asm_cpu_state(void): which parts of the ABI state were wrong on entry.
.SECTION "asm_cpu_state" SUPERFREE
asm_cpu_state:
  php
  rep #$30
  stz.b tcc__r0
  sep #$20
  pla                 ; P as it was on entry
  and.b #$30
  sta.b tcc__r0
  rep #$20
  tdc
  beq +
  lda.b tcc__r0
  ora.w #$0100
  sta.b tcc__r0
+ sep #$20
  phb
  pla
  cmp.b #$7e
  rep #$20
  beq +
  lda.b tcc__r0
  ora.w #$0200
  sta.b tcc__r0
+ rep #$30
  rtl
.ENDS
