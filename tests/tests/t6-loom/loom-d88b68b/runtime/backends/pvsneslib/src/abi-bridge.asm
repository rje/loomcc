.include "hdr.asm"

.accu 16
.index 16
.16bit

; Low RAM lives in bank $00 whatever the image's code base: `.BASE` from
; hdr.asm would otherwise stamp these labels with the code bank, which in
; HiROM ($C0) is cartridge, not RAM.
.BASE $00
.RAMSECTION "loom.pvs.abi.scratch" BANK 0 SLOT 1
loom_pvs_abi_byte dsb 1
loom_pvs_abi_signed dsb 1
loom_pvs_abi_left dsb 2
loom_pvs_abi_right dsb 2
loom_pvs_abi_record dsb 4
loom_pvs_abi_result_u8 dsb 1
loom_pvs_abi_result_s8 dsb 1
loom_pvs_abi_result_u16 dsb 2
.ENDS
.BASE LOOM_ROM_BASE

.SECTION "loom.pvs.abi.bridge" SUPERFREE KEEP

; 816-tcc uses JSL/RTL, places the first argument at 4,s, packs 8-bit
; arguments as one byte, and represents data pointers as four bytes.
loom_conformance_abi_roundtrip:
  ; Preserve the caller's processor mode before changing accumulator/index
  ; widths. The saved status byte shifts every incoming argument by one byte.
  php
  rep #$30

  sep #$20
  lda 5,s
  sta.l loom_pvs_abi_byte
  lda 6,s
  sta.l loom_pvs_abi_signed
  rep #$20
  lda 7,s
  sta.l loom_pvs_abi_left
  lda 9,s
  sta.l loom_pvs_abi_right
  lda 11,s
  sta.l loom_pvs_abi_record
  lda 13,s
  sta.l loom_pvs_abi_record+2

  ; The 816-tcc result registers are caller-clobbered, but the surrounding CPU
  ; state follows the PVSnesLib assembly convention for C-callable routines.
  phb
  phx
  phy

  ; callback_u8(byte_value, 0x13)
  sep #$20
  lda #$13
  pha
  lda.l loom_pvs_abi_byte
  pha
  rep #$20
  jsl loom_conformance_abi_callback_u8
  pla
  sep #$20
  lda.b tcc__r0
  sta.l loom_pvs_abi_result_u8

  ; callback_s8(signed_value, 7)
  lda #$07
  pha
  lda.l loom_pvs_abi_signed
  pha
  rep #$20
  jsl loom_conformance_abi_callback_s8
  pla
  sep #$20
  lda.b tcc__r0
  sta.l loom_pvs_abi_result_s8

  ; callback_u16(left, right)
  rep #$20
  lda.l loom_pvs_abi_right
  pha
  lda.l loom_pvs_abi_left
  pha
  jsl loom_conformance_abi_callback_u16
  pla
  pla
  lda.b tcc__r0
  sta.l loom_pvs_abi_result_u16

  ; Reload the far record pointer after the C callbacks clobber imaginary
  ; registers, then write the exact ten-byte record layout.
  lda.l loom_pvs_abi_record
  sta.b tcc__r2
  lda.l loom_pvs_abi_record+2
  sta.b tcc__r2h
  ldy #0
  lda.l loom_pvs_abi_left
  sta.b [tcc__r2],y
  ldy #2
  lda.l loom_pvs_abi_right
  sta.b [tcc__r2],y
  ldy #4
  lda.l loom_pvs_abi_result_u16
  sta.b [tcc__r2],y
  sep #$20
  ldy #6
  lda.l loom_pvs_abi_byte
  sta.b [tcc__r2],y
  iny
  lda.l loom_pvs_abi_signed
  sta.b [tcc__r2],y
  iny
  lda.l loom_pvs_abi_result_u8
  sta.b [tcc__r2],y
  iny
  lda.l loom_pvs_abi_result_s8
  sta.b [tcc__r2],y

  rep #$30
  lda.l loom_pvs_abi_result_u16
  eor #$a55a
  sta.b tcc__r0
  ply
  plx
  plb
  plp
  rtl

.ENDS
