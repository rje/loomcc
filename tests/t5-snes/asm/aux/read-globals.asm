.include "hdr.asm"
.accu 16
.index 16
.16bit
.SECTION "asm_read_globals" SUPERFREE
asm_sum_x_plus_y:
  rep #$30
  stz.b tcc__r0
  ldx.w #0
  ldy.w body_count
- cpy.w #0
  beq +
  lda.w bodies+0,x
  clc
  adc.w bodies+8,x
  clc
  adc.b tcc__r0
  sta.b tcc__r0
  txa
  clc
  adc.w #12
  tax
  dey
  bra -
+ rtl

asm_set_flags:
  rep #$30
  ldx.w #0
  ldy.w body_count
- cpy.w #0
  beq +
  sep #$20
  lda 4,s
  sta.w bodies+2,x
  rep #$20
  txa
  clc
  adc.w #12
  tax
  dey
  bra -
+ rep #$30
  rtl
.ENDS
