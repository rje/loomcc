.include "hdr.asm"

.accu 16
.index 16
.16bit

; The console-side OAM path of the frame transaction, in assembly because
; 816-tcc spends roughly fifteen scanlines per sprite on the same work in C.
; Entries are validated, checked for duplicate slots within a commit, and
; written straight into pvsneslib's OAM shadow (oamMemory); pvsneslib's NMI
; handler uploads the shadow only from WaitForVBlank, which the adapter calls
; once per presented commit, so writes during the tick never tear.
;
; The host build keeps the portable C path in frame-transaction.c and
; runtime-adapter.c; both implement the same contract.
;
; 816-tcc ABI: JSL/RTL, the first argument at 4,s (5,s after our php), data
; pointers as four bytes (offset word, bank word), 8-bit results in tcc__r0.
; (loomcc testbed: `.BASE $00` around the RAM section as Loom HEAD writes it,
; so the RAM symbols stay in bank $00 rather than $80; hdr.asm leaves .BASE $80.)
.BASE $00
.RAMSECTION "loom.pvs.oam.state" BANK 0 SLOT 1
loom_pvs_oam_generation dsb 1
loom_pvs_oam_count dsb 1
loom_pvs_oam_previous_count dsb 1
loom_pvs_oam_pad dsb 1
loom_pvs_oam_mark dsb 128
loom_pvs_oam_current dsb 34
loom_pvs_oam_previous dsb 34
.ENDS
.BASE $80

.SECTION "loom.pvs.oam.tables" SUPERFREE KEEP
; High-table bit positions for slot & 3.
loom_pvs_oam_high_keep:
  .db $fc,$f3,$cf,$3f
loom_pvs_oam_high_x:
  .db $01,$04,$10,$40
loom_pvs_oam_high_large:
  .db $02,$08,$20,$80
.ENDS

.SECTION "loom.pvs.oam.code" SUPERFREE KEEP

; LoomStatus loom_pvs_oam_stage(const LoomOamEntry *entry)
; LoomOamEntry: x s16 @0, y s16 @2, tile u16 @4, slot @6, palette @7,
; priority @8, size @9, flags @10, reserved @11.
loom_pvs_oam_stage:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r1
  lda 7,s
  sta.b tcc__r1h
  ; slot < 128
  ldy #6
  lda [tcc__r1],y
  and #$00ff
  cmp #128
  bcs loom_pvs_oam_stage_invalid
  tax
  ; tile < 512
  ldy #4
  lda [tcc__r1],y
  cmp #512
  bcs loom_pvs_oam_stage_invalid
  ; x in [-256, 255]
  ldy #0
  lda [tcc__r1],y
  clc
  adc #256
  cmp #512
  bcs loom_pvs_oam_stage_invalid
  ; y in [-64, 255]
  ldy #2
  lda [tcc__r1],y
  clc
  adc #64
  cmp #320
  bcs loom_pvs_oam_stage_invalid
  sep #$20
  ; palette < 8
  ldy #7
  lda [tcc__r1],y
  cmp #8
  bcs loom_pvs_oam_stage_invalid
  ; priority < 4
  ldy #8
  lda [tcc__r1],y
  cmp #4
  bcs loom_pvs_oam_stage_invalid
  ; size < 2
  ldy #9
  lda [tcc__r1],y
  cmp #2
  bcs loom_pvs_oam_stage_invalid
  ; flags within flip bits, reserved zero
  ldy #10
  lda [tcc__r1],y
  and #$fc
  bne loom_pvs_oam_stage_invalid
  ldy #11
  lda [tcc__r1],y
  bne loom_pvs_oam_stage_invalid
  bra loom_pvs_oam_stage_accept
loom_pvs_oam_stage_invalid:
  rep #$20
  lda #3
  sta.b tcc__r0
  plp
  rtl
loom_pvs_oam_stage_accept:
  ; duplicate slot within this commit
  lda.l loom_pvs_oam_mark,x
  cmp.l loom_pvs_oam_generation
  beq loom_pvs_oam_stage_invalid
  lda.l loom_pvs_oam_generation
  sta.l loom_pvs_oam_mark,x
  ; current[count] = slot; ++count
  stx.b tcc__r2
  rep #$20
  lda.l loom_pvs_oam_count
  and #$00ff
  tax
  sep #$20
  lda.b tcc__r2
  sta.l loom_pvs_oam_current,x
  lda.l loom_pvs_oam_count
  inc a
  sta.l loom_pvs_oam_count
  ; low table: X = slot * 4
  rep #$20
  lda.b tcc__r2
  and #$00ff
  asl a
  asl a
  tax
  sep #$20
  ldy #0
  lda [tcc__r1],y
  sta.l oamMemory,x
  ldy #2
  lda [tcc__r1],y
  sta.l oamMemory+1,x
  ldy #4
  lda [tcc__r1],y
  sta.l oamMemory+2,x
  ; attributes: flipY<<7 | flipX<<6 | priority<<4 | palette<<1 | tile bit 8
  ldy #5
  lda [tcc__r1],y
  and #$01
  sta.b tcc__r3
  ldy #7
  lda [tcc__r1],y
  asl a
  ora.b tcc__r3
  sta.b tcc__r3
  ldy #8
  lda [tcc__r1],y
  asl a
  asl a
  asl a
  asl a
  ora.b tcc__r3
  sta.b tcc__r3
  ldy #10
  lda [tcc__r1],y
  asl a
  asl a
  asl a
  asl a
  asl a
  asl a
  ora.b tcc__r3
  sta.l oamMemory+3,x
  ; high table: quarter bits from the tables, then the byte at slot >> 2
  rep #$20
  lda.b tcc__r2
  and #$0003
  tax
  sep #$20
  lda.l loom_pvs_oam_high_keep,x
  sta.b tcc__r3
  lda.l loom_pvs_oam_high_x,x
  sta.b tcc__r4
  lda.l loom_pvs_oam_high_large,x
  sta.b tcc__r5
  rep #$20
  lda.b tcc__r2
  and #$00ff
  lsr a
  lsr a
  tax
  sep #$20
  lda.l oamMemory+512,x
  and.b tcc__r3
  sta.b tcc__r3
  ldy #1
  lda [tcc__r1],y
  bpl loom_pvs_oam_stage_x_positive
  lda.b tcc__r3
  ora.b tcc__r4
  sta.b tcc__r3
loom_pvs_oam_stage_x_positive:
  ldy #9
  lda [tcc__r1],y
  beq loom_pvs_oam_stage_small
  lda.b tcc__r3
  ora.b tcc__r5
  sta.b tcc__r3
loom_pvs_oam_stage_small:
  lda.b tcc__r3
  sta.l oamMemory+512,x
  rep #$20
  lda #0
  sta.b tcc__r0
  plp
  rtl

.ENDS
