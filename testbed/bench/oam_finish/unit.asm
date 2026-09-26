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

; void loom_pvs_oam_finish(void): park every slot written last commit that
; this commit did not stage (x = -256, y = 240), then remember this commit's
; slots as the previous set.
loom_pvs_oam_finish:
  php
  rep #$30
  lda.l loom_pvs_oam_previous_count
  and #$00ff
  sta.b tcc__r3
  stz.b tcc__r4
loom_pvs_oam_finish_hide_loop:
  ldx.b tcc__r4
  cpx.b tcc__r3
  bcs loom_pvs_oam_finish_hide_done
  sep #$20
  lda.l loom_pvs_oam_previous,x
  rep #$20
  and #$00ff
  sta.b tcc__r2
  tax
  sep #$20
  lda.l loom_pvs_oam_mark,x
  cmp.l loom_pvs_oam_generation
  beq loom_pvs_oam_finish_hide_next
  rep #$20
  lda.b tcc__r2
  asl a
  asl a
  tax
  sep #$20
  lda #0
  sta.l oamMemory,x
  lda #240
  sta.l oamMemory+1,x
  rep #$20
  lda.b tcc__r2
  and #$0003
  tax
  sep #$20
  lda.l loom_pvs_oam_high_keep,x
  sta.b tcc__r5
  lda.l loom_pvs_oam_high_x,x
  sta.b tcc__r5+1
  rep #$20
  lda.b tcc__r2
  lsr a
  lsr a
  tax
  sep #$20
  lda.l oamMemory+512,x
  and.b tcc__r5
  ora.b tcc__r5+1
  sta.l oamMemory+512,x
loom_pvs_oam_finish_hide_next:
  rep #$20
  inc.b tcc__r4
  bra loom_pvs_oam_finish_hide_loop
loom_pvs_oam_finish_hide_done:
  ; previous = current
  rep #$20
  lda.l loom_pvs_oam_count
  and #$00ff
  sta.b tcc__r3
  ldx #0
loom_pvs_oam_finish_copy_loop:
  cpx.b tcc__r3
  bcs loom_pvs_oam_finish_copy_done
  sep #$20
  lda.l loom_pvs_oam_current,x
  sta.l loom_pvs_oam_previous,x
  rep #$20
  inx
  bra loom_pvs_oam_finish_copy_loop
loom_pvs_oam_finish_copy_done:
  sep #$20
  lda.l loom_pvs_oam_count
  sta.l loom_pvs_oam_previous_count
  lda #0
  sta.l loom_pvs_oam_count
  rep #$20
  plp
  rtl

.ENDS
