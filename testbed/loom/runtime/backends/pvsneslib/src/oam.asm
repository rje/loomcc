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
;
; The two record strides below have to match the C structs exactly: this file
; walks the arrays by hand, so a field added in mode1.h and not mirrored here
; makes every record after the first read the previous one's tail. The header
; declares the same two numbers as LOOM_MODE1_SPRITE_BYTES and
; LOOM_MODE1_SPRITE_POSE_BYTES, C asserts them against sizeof, and
; `oam_assembly_strides_match_the_runtime_structs` compares them to these.

.DEFINE LOOM_MODE1_SPRITE_BYTES 20
.DEFINE LOOM_MODE1_SPRITE_POSE_BYTES 12

; Low RAM stays in bank $00 whatever `.BASE` hdr.asm set (see abi-bridge.asm).
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

; Per slot: 1 while it holds the tile, attributes and size bit the batch
; last wrote for it (cleared whenever the finish hides the slot, and at init).
; In bank $7e: bank 0's low RAM is the direct page SNESMOD's variables need.
.RAMSECTION "loom.pvs.oam.staged" BANK $7e SLOT 2
loom_pvs_oam_staged dsb 128
.ENDS
.BASE LOOM_ROM_BASE

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

; void loom_pvs_oam_init(void): no slot is marked, nothing was written.
loom_pvs_oam_init:
  php
  rep #$30
  sep #$20
  lda #0
  sta.l loom_pvs_oam_generation
  sta.l loom_pvs_oam_count
  sta.l loom_pvs_oam_previous_count
  lda #$ff
  ldx #0
loom_pvs_oam_init_loop:
  sta.l loom_pvs_oam_mark,x
  lda #0
  sta.l loom_pvs_oam_staged,x
  lda #$ff
  inx
  cpx #128
  bne loom_pvs_oam_init_loop
  plp
  rtl

; void loom_pvs_oam_begin(void): the commit opens at submit, after the Mode 1
; build may already have batched its sprites, so the generation and the
; count are owned by loom_pvs_oam_finish (and reset by loom_pvs_oam_abort).
loom_pvs_oam_begin:
  rtl

; void loom_pvs_oam_abort(void): a commit dropped after staging. The marks
; already carry this generation, so merely zeroing the count left the next
; commit's staging of the same slots refused as duplicates (a busy port at a
; room swap halted Hilltop Run, GAME-001). What was staged is finished as if
; committed: the shadow already holds it, the previous set follows it, and
; the generation moves on. Nothing staged: nothing to do.
loom_pvs_oam_abort:
  php
  sep #$20
  lda.l loom_pvs_oam_count
  beq loom_pvs_oam_abort_done
  plp
  jmp loom_pvs_oam_finish
loom_pvs_oam_abort_done:
  plp
  rtl

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
  lda #0
  sta.l loom_pvs_oam_staged,x           ; the next staging writes it whole
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
  ; the next commit stages under the next generation; $ff never marks a slot
  lda.l loom_pvs_oam_generation
  inc a
  cmp #$ff
  bne loom_pvs_oam_finish_generation
  ; The generation wraps. A slot last staged exactly 255 commits ago still
  ; carries a mark equal to the generation about to come round again, and
  ; staging it then read as a duplicate: Hilltop Run's overworld marker
  ; took the slot two collected coins had left, 255 commits later, and
  ; the swap halted (GAME-001). Forget every mark instead: 128 bytes,
  ; once every 255 commits.
  lda #$ff
  ldx #0
loom_pvs_oam_finish_forget_loop:
  sta.l loom_pvs_oam_mark,x
  inx
  cpx #128
  bne loom_pvs_oam_finish_forget_loop
  lda #0
loom_pvs_oam_finish_generation:
  sta.l loom_pvs_oam_generation
  rep #$20
  plp
  rtl

; LoomStatus loom_pvs_oam_batch(const LoomMode1SpriteBatch *batch)
; The Mode 1 sprite build: for every visible sprite compute the screen
; position from the world position, the camera and the pose pivot, cull
; against the view, then stage it exactly as loom_pvs_oam_stage would.
; LoomMode1SpriteBatch: sprites @0, poses @4, visible @8, world_x @12,
; world_y @16 (four-byte pointers), camera_x @20, camera_y @22, count @24.
; LoomMode1Sprite: tile @8, slot @10, palette @11, priority @12,
; size @13, flags @14. LoomMode1SpritePose: pivot_x @0,
; pivot_y @2, tile @4, palette @6, size @7, width @8, height @9, flags @10.
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
  ; index the list by count, not by the slot still in A: with the slot
  ; as the index the list read 0, 1, 2, ... whatever was staged, and a
  ; room with fewer sprites than the last never parked the higher slots
  ; (GAME-001 found the level's hero and walker standing on the overworld).
  rep #$20
  lda.l loom_pvs_oam_count
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
  ; A sprite whose pose is unchanged and whose slot still holds what that
  ; pose wrote needs only its position: the tile, the attributes and the
  ; size bit are already in the shadow.
  phx
  rep #$20
  lda.b tcc__r2h+1
  and #$00ff
  tax                                   ; the slot
  sep #$20
  lda.l loom_pvs_oam_staged,x
  beq loom_pvs_oam_batch_full
  ldx.b tcc__r9                         ; the sprite
  lda.l loom_mode1_pose_dirty,x
  bne loom_pvs_oam_batch_full_dirty
  plx
  brl loom_pvs_oam_batch_fast_high
loom_pvs_oam_batch_full:
  lda #1
  sta.l loom_pvs_oam_staged,x
  ldx.b tcc__r9
loom_pvs_oam_batch_full_dirty:
  lda #0
  sta.l loom_mode1_pose_dirty,x
  plx
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
  ; flips: the authored sprite's flags plus the pose's mirror
  ldy #10
  lda [tcc__r3],y
  ldy #14
  ora [tcc__r2],y
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
  brl loom_pvs_oam_batch_next
loom_pvs_oam_batch_fast_high:
  ; only this slot's x-sign bit in the high table can have changed
  rep #$20
  lda.b tcc__r2h+1
  and #$0003
  tax
  sep #$20
  lda.l loom_pvs_oam_high_x,x
  sta.b tcc__r4h+1
  eor #$ff
  sta.b tcc__r3h+1
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
  bpl +
  lda.b tcc__r3h+1
  ora.b tcc__r4h+1
  sta.b tcc__r3h+1
+ lda.b tcc__r3h+1
  sta.l oamMemory+512,x
  brl loom_pvs_oam_batch_next
loom_pvs_oam_batch_next:
  rep #$20
  inc.b tcc__r9
  lda.b tcc__r2
  clc
  adc #LOOM_MODE1_SPRITE_BYTES
  sta.b tcc__r2
  lda.b tcc__r3
  clc
  adc #LOOM_MODE1_SPRITE_POSE_BYTES
  sta.b tcc__r3
  brl loom_pvs_oam_batch_loop
loom_pvs_oam_batch_done:
  rep #$20
  stz.b tcc__r0
  plp
  rtl

.ENDS

; ---------------------------------------------------------------------------
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
.BASE LOOM_ROM_BASE

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

; ---------------------------------------------------------------------------
; The frame build's display block (PERF-004), after the scroll: backdrop,
; brightness, the main layers, the sprite size pair, the scene's raster
; binding (off while raster is disabled or the scene is loading) and the
; forced blank of a dark screen. What stays C is reported for C to call:
; colour math and the raster program's per-frame drive.
;
; loom_u16 loom_pvs_mode1_display(LoomDisplayState *display 5,s;
;     LoomRasterBinding *raster 9,s; const LoomMode1Scene *scene 13,s;
;     loom_u16 bits 17,s)
; bits: has_bg2 1, has_bg3 2, ready 4, raster_enabled 8, brightness << 8.
; r0 <- lit 1 | colour math due 2 | raster drive due 4.
;
; LoomDisplayState: backdrop_color @16, brightness @21, main_layers @22,
; obj_size_pair @24, flags @29. LoomMode1Scene (asserted 68 bytes in
; mode1.c: 816-tcc aligns its pointers to four): backdrop_color @16, raster
; @18 (program, state), obj_size_pair @22, color_math_mode @65.

.SECTION "loom.pvs.mode1.display.code" SUPERFREE KEEP

loom_pvs_mode1_display:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r1
  lda 7,s
  sta.b tcc__r1h                        ; display
  lda 9,s
  sta.b tcc__r2
  lda 11,s
  sta.b tcc__r2h                        ; raster
  lda 13,s
  sta.b tcc__r3
  lda 15,s
  sta.b tcc__r3h                        ; scene
  lda 17,s
  sta.b tcc__r4                         ; bits
  stz.b tcc__r0
  ; lit = ready and a nonzero brightness
  and #$0004
  beq +
  lda.b tcc__r4
  and #$ff00
  beq +
  lda #1
  sta.b tcc__r0
+ ; backdrop_color
  ldy #16
  lda [tcc__r3],y
  sta [tcc__r1],y
  sep #$20
  ; brightness: the scene's while ready, else dark
  lda.b tcc__r4
  and #$04
  beq +
  lda.b tcc__r4+1
+ ldy #21
  sta [tcc__r1],y
  ; main layers: BG1 and OBJ, BG2 and BG3 when driven, none while dark
  lda.b tcc__r0
  beq ++
  lda.b tcc__r4
  and #$03                              ; has_bg2 1 -> BG2 2, has_bg3 2 -> BG3 4
  asl a
  ora #$11                              ; LOOM_LAYER_BG1 | LOOM_LAYER_OBJ
++ ldy #22
  sta [tcc__r1],y
  ; the sprite size pair
  ldy #22
  lda [tcc__r3],y
  ldy #24
  sta [tcc__r1],y
  ; forced blank while dark
  lda.b tcc__r0
  eor #$01
  ldy #29
  sta [tcc__r1],y
  ; colour math is due on a lit screen whose scene asks for it
  lda.b tcc__r0
  beq +
  ldy #65
  lda [tcc__r3],y
  beq +
  lda.b tcc__r0
  ora #$02
  sta.b tcc__r0
+ rep #$20
  ; the raster binding, off while raster is disabled or the scene loads
  lda.b tcc__r4
  and #$000c
  cmp #$000c
  beq +
  lda #$ffff
  sta [tcc__r2]
  ldy #2
  sta [tcc__r2],y
  bra loom_pvs_mode1_display_done
+ ldy #18
  lda [tcc__r3],y
  sta [tcc__r2]
  cmp #$ffff
  beq +
  lda.b tcc__r0
  ora #$0004
  sta.b tcc__r0
+ ldy #20
  lda [tcc__r3],y
  ldy #2
  sta [tcc__r2],y
loom_pvs_mode1_display_done:
  lda.b tcc__r0
  and #$00ff
  sta.b tcc__r0
  plp
  rtl

.ENDS
