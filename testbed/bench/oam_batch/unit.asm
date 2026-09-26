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

; LoomStatus loom_pvs_oam_batch(const LoomMode1SpriteBatch *batch)
; The Mode 1 sprite build: for every visible sprite compute the screen
; position from the world position, the camera and the pose pivot, cull
; against the view, then stage it exactly as loom_pvs_oam_stage would.
; LoomMode1SpriteBatch: sprites @0, poses @4, visible @8, world_x @12,
; world_y @16 (four-byte pointers), camera_x @20, camera_y @22, count @24.
; LoomMode1Sprite (18 bytes): tile @8, slot @10, palette @11, priority @12,
; size @13, flags @14. LoomMode1SpritePose (12 bytes): pivot_x @0,
; pivot_y @2, tile @4, palette @6, size @7, width @8, height @9.
; Scratch: r1 batch then world_y, r2 sprite, r3 pose, r4 visible,
; r5 world_x, r9 index, r9h camera_x, r10 count, r10h camera_y, r0 sx,
; r0h sy; the spare high bytes of r2h..r5h hold the slot and attribute bits.
loom_pvs_oam_batch:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r1
  lda 7,s
  sta.b tcc__r1h
  ldy #0
  lda [tcc__r1],y
  sta.b tcc__r2
  ldy #2
  lda [tcc__r1],y
  sta.b tcc__r2h
  ldy #4
  lda [tcc__r1],y
  sta.b tcc__r3
  ldy #6
  lda [tcc__r1],y
  sta.b tcc__r3h
  ldy #8
  lda [tcc__r1],y
  sta.b tcc__r4
  ldy #10
  lda [tcc__r1],y
  sta.b tcc__r4h
  ldy #12
  lda [tcc__r1],y
  sta.b tcc__r5
  ldy #14
  lda [tcc__r1],y
  sta.b tcc__r5h
  ldy #20
  lda [tcc__r1],y
  sta.b tcc__r9h
  ldy #22
  lda [tcc__r1],y
  sta.b tcc__r10h
  ldy #24
  lda [tcc__r1],y
  and #$00ff
  sta.b tcc__r10
  ; world_y last: it replaces the batch pointer
  ldy #16
  lda [tcc__r1],y
  tax
  ldy #18
  lda [tcc__r1],y
  sta.b tcc__r1h
  stx.b tcc__r1
  stz.b tcc__r9
loom_pvs_oam_batch_loop:
  lda.b tcc__r9
  cmp.b tcc__r10
  bcc loom_pvs_oam_batch_body
  brl loom_pvs_oam_batch_done
loom_pvs_oam_batch_body:
  ; visible[i]
  tay
  sep #$20
  lda [tcc__r4],y
  rep #$20
  and #$00ff
  bne loom_pvs_oam_batch_visible
  brl loom_pvs_oam_batch_next
loom_pvs_oam_batch_visible:
  ; sx = world_x[i] - camera_x - pivot_x
  lda.b tcc__r9
  asl a
  tay
  lda [tcc__r5],y
  sec
  sbc.b tcc__r9h
  ldy #0
  sec
  sbc [tcc__r3],y
  sta.b tcc__r0
  ; sx + width > 0
  ldy #8
  sep #$20
  lda [tcc__r3],y
  rep #$20
  and #$00ff
  clc
  adc.b tcc__r0
  beq loom_pvs_oam_batch_skip
  bmi loom_pvs_oam_batch_skip
  ; sx < 256 (a negative sx is on screen)
  lda.b tcc__r0
  bmi loom_pvs_oam_batch_x_ok
  cmp #256
  bcs loom_pvs_oam_batch_skip
loom_pvs_oam_batch_x_ok:
  ; sy = world_y[i] - camera_y - pivot_y
  lda.b tcc__r9
  asl a
  tay
  lda [tcc__r1],y
  sec
  sbc.b tcc__r10h
  ldy #2
  sec
  sbc [tcc__r3],y
  sta.b tcc__r0h
  ldy #9
  sep #$20
  lda [tcc__r3],y
  rep #$20
  and #$00ff
  clc
  adc.b tcc__r0h
  beq loom_pvs_oam_batch_skip
  bmi loom_pvs_oam_batch_skip
  lda.b tcc__r0h
  bmi loom_pvs_oam_batch_y_ok
  cmp #224
  bcs loom_pvs_oam_batch_skip
loom_pvs_oam_batch_y_ok:
  bra loom_pvs_oam_batch_stage
loom_pvs_oam_batch_skip:
  brl loom_pvs_oam_batch_next
loom_pvs_oam_batch_stage:
  ; slot from the sprite record
  ldy #10
  lda [tcc__r2],y
  and #$00ff
  cmp #128
  bcs loom_pvs_oam_batch_invalid
  tax
  sep #$20
  lda.l loom_pvs_oam_mark,x
  cmp.l loom_pvs_oam_generation
  beq loom_pvs_oam_batch_invalid
  lda.l loom_pvs_oam_generation
  sta.l loom_pvs_oam_mark,x
  ; current[count] = slot; ++count (capacity 33)
  lda.l loom_pvs_oam_count
  cmp #33
  bcs loom_pvs_oam_batch_capacity
  bra loom_pvs_oam_batch_accept
loom_pvs_oam_batch_invalid:
  rep #$20
  lda #3
  sta.b tcc__r0
  plp
  rtl
loom_pvs_oam_batch_capacity:
  rep #$20
  lda #2
  sta.b tcc__r0
  plp
  rtl
loom_pvs_oam_batch_accept:
  txa
  sta.b tcc__r2h+1
  rep #$20
  and #$00ff
  tax
  sep #$20
  lda.b tcc__r2h+1
  sta.l loom_pvs_oam_current,x
  lda.l loom_pvs_oam_count
  inc a
  sta.l loom_pvs_oam_count
  ; low table at slot * 4
  rep #$20
  lda.b tcc__r2h+1
  and #$00ff
  asl a
  asl a
  tax
  sep #$20
  lda.b tcc__r0
  sta.l oamMemory,x
  lda.b tcc__r0h
  sta.l oamMemory+1,x
  ldy #4
  lda [tcc__r3],y
  sta.l oamMemory+2,x
  ; attributes: flags<<6 | priority<<4 | palette<<1 | tile bit 8
  ldy #5
  lda [tcc__r3],y
  and #$01
  sta.b tcc__r3h+1
  ldy #6
  lda [tcc__r3],y
  asl a
  ora.b tcc__r3h+1
  sta.b tcc__r3h+1
  ldy #12
  lda [tcc__r2],y
  asl a
  asl a
  asl a
  asl a
  ora.b tcc__r3h+1
  sta.b tcc__r3h+1
  ldy #14
  lda [tcc__r2],y
  asl a
  asl a
  asl a
  asl a
  asl a
  asl a
  ora.b tcc__r3h+1
  sta.l oamMemory+3,x
  ; high table bits
  rep #$20
  lda.b tcc__r2h+1
  and #$0003
  tax
  sep #$20
  lda.l loom_pvs_oam_high_keep,x
  sta.b tcc__r3h+1
  lda.l loom_pvs_oam_high_x,x
  sta.b tcc__r4h+1
  lda.l loom_pvs_oam_high_large,x
  sta.b tcc__r5h+1
  rep #$20
  lda.b tcc__r2h+1
  and #$00ff
  lsr a
  lsr a
  tax
  sep #$20
  lda.l oamMemory+512,x
  and.b tcc__r3h+1
  sta.b tcc__r3h+1
  lda.b tcc__r0+1
  bpl loom_pvs_oam_batch_x_positive
  lda.b tcc__r3h+1
  ora.b tcc__r4h+1
  sta.b tcc__r3h+1
loom_pvs_oam_batch_x_positive:
  ldy #7
  lda [tcc__r3],y
  beq loom_pvs_oam_batch_small
  lda.b tcc__r3h+1
  ora.b tcc__r5h+1
  sta.b tcc__r3h+1
loom_pvs_oam_batch_small:
  lda.b tcc__r3h+1
  sta.l oamMemory+512,x
loom_pvs_oam_batch_next:
  rep #$20
  inc.b tcc__r9
  lda.b tcc__r2
  clc
  adc #18
  sta.b tcc__r2
  lda.b tcc__r3
  clc
  adc #12
  sta.b tcc__r3
  brl loom_pvs_oam_batch_loop
loom_pvs_oam_batch_done:
  rep #$20
  stz.b tcc__r0
  plp
  rtl

.ENDS
