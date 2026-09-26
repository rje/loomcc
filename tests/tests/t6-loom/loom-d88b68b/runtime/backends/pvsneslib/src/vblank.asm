.include "hdr.asm"

.accu 16
.index 16
.16bit

; The VBlank side of a presented frame, in assembly. The display registers
; and the pad sample sit between the NMI and the tick's start; through
; 816-tcc they took twenty scanlines of the frame's 262, which with a tick
; that fits one frame was enough to miss the next NMI and run at half rate.
;
; 816-tcc ABI: JSL/RTL, the first argument at 4,s (5,s after our php),
; pointers as four bytes (offset word, bank word). Direct page is zero, so
; tcc__r0/tcc__r1 are the scratch words the C compiler also uses.
;
; The host build keeps the portable C in runtime-adapter.c; both implement
; the same register writes, in the same order.

.SECTION "loom.pvs.vblank.code" SUPERFREE KEEP

; void loom_pvs_display_apply(const LoomDisplayState *display)
; LoomDisplayState: bg_scroll_x[4] s16 @0, bg_scroll_y[4] s16 @8,
; backdrop_color u16 @16, fixed_color u16 @18, mode @20, brightness @21,
; main_layers @22, sub_layers @23, obj_size_pair @24, mosaic_size @25,
; mosaic_layers @26, color_math_layers @27, color_math_flags @28, flags @29.
; Flags: FORCED_BLANK 1, MODE1_BG3_PRIORITY 2; colour math flags:
; SUBTRACT 1, HALF 2, USE_FIXED_COLOR 4.
loom_pvs_display_apply:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r1
  lda 7,s
  sta.b tcc__r1h
  sep #$20
  ; BGMODE ($2105): the mode, with BG3 priority when the flag says so
  ldy #29
  lda [tcc__r1],y
  and #$02
  beq +
  lda #$08
+ sta.b tcc__r0
  ldy #20
  lda [tcc__r1],y
  ora.b tcc__r0
  sta.l $002105
  ; MOSAIC ($2106): 0, or (size - 1) << 4 | layers & 15
  ldy #25
  lda [tcc__r1],y
  beq +
  dec a
  asl a
  asl a
  asl a
  asl a
  sta.b tcc__r0
  ldy #26
  lda [tcc__r1],y
  and #$0f
  ora.b tcc__r0
+ sta.l $002106
  ; the scroll registers, each written low byte then high
  ldy #0
  lda [tcc__r1],y
  sta.l $00210d
  iny
  lda [tcc__r1],y
  sta.l $00210d
  ldy #8
  lda [tcc__r1],y
  sta.l $00210e
  iny
  lda [tcc__r1],y
  sta.l $00210e
  ldy #2
  lda [tcc__r1],y
  sta.l $00210f
  iny
  lda [tcc__r1],y
  sta.l $00210f
  ldy #10
  lda [tcc__r1],y
  sta.l $002110
  iny
  lda [tcc__r1],y
  sta.l $002110
  ldy #4
  lda [tcc__r1],y
  sta.l $002111
  iny
  lda [tcc__r1],y
  sta.l $002111
  ldy #12
  lda [tcc__r1],y
  sta.l $002112
  iny
  lda [tcc__r1],y
  sta.l $002112
  ldy #6
  lda [tcc__r1],y
  sta.l $002113
  iny
  lda [tcc__r1],y
  sta.l $002113
  ldy #14
  lda [tcc__r1],y
  sta.l $002114
  iny
  lda [tcc__r1],y
  sta.l $002114
  ; the backdrop: CGRAM entry 0
  lda #0
  sta.l $002121
  ldy #16
  lda [tcc__r1],y
  sta.l $002122
  iny
  lda [tcc__r1],y
  and #$7f
  sta.l $002122
  ; COLDATA ($2132): the fixed colour's three components
  rep #$20
  ldy #18
  lda [tcc__r1],y
  sta.b tcc__r0
  and #$001f
  ora #$0020
  sep #$20
  sta.l $002132
  rep #$20
  lda.b tcc__r0
  lsr a
  lsr a
  lsr a
  lsr a
  lsr a
  and #$001f
  ora #$0040
  sep #$20
  sta.l $002132
  rep #$20
  lda.b tcc__r0
  lsr a
  lsr a
  lsr a
  lsr a
  lsr a
  lsr a
  lsr a
  lsr a
  lsr a
  lsr a
  and #$001f
  ora #$0080
  sep #$20
  sta.l $002132
  ; TM and TS
  ldy #22
  lda [tcc__r1],y
  and #$1f
  sta.l $00212c
  ldy #23
  lda [tcc__r1],y
  and #$1f
  sta.l $00212d
  ; CGWSEL ($2130): 0 with the fixed colour, else 2
  ldy #28
  lda [tcc__r1],y
  and #$04
  eor #$04
  lsr a
  sta.l $002130
  ; CGADSUB ($2131): layers, half at bit 6, subtract at bit 7
  ldy #27
  lda [tcc__r1],y
  and #$3f
  sta.b tcc__r0
  ldy #28
  lda [tcc__r1],y
  and #$02
  beq +
  lda #$40
+ ora.b tcc__r0
  sta.b tcc__r0
  ldy #28
  lda [tcc__r1],y
  and #$01
  beq +
  lda #$80
+ ora.b tcc__r0
  sta.l $002131
  ; OBSEL ($2101): the size pair
  ldy #24
  lda [tcc__r1],y
  and #$07
  asl a
  asl a
  asl a
  asl a
  asl a
  sta.l $002101
  ; INIDISP ($2100): brightness, forced blank at bit 7; pvsneslib's mirror
  ldy #21
  lda [tcc__r1],y
  and #$0f
  sta.b tcc__r0
  ldy #29
  lda [tcc__r1],y
  and #$01
  beq +
  lda #$80
+ ora.b tcc__r0
  sta.l $002100
  sta.l mirrorINIDISP
  plp
  rtl

; void loom_pvs_inputs_sample(void): once the automatic joypad read is
; done, fold each pad's state into the held, pressed and released words
; (u16 per pad, two pads) the runtime adapter keeps.
loom_pvs_inputs_sample:
  php
  rep #$30
  sep #$20
- lda.l $004212
  and #$01
  bne -
  rep #$20
  lda.l $004218
  ldx #0
  jsr loom_pvs_inputs_pad
  lda.l $00421a
  ldx #2
  jsr loom_pvs_inputs_pad
  plp
  rtl

; A: the pad's word; X: its byte offset in the three arrays. A standard pad
; answers 0000 in its low nibble; anything else is an empty port or a mouse
; and must not read as every button held.
loom_pvs_inputs_pad:
  sta.b tcc__r0
  and #$000f
  beq +
  stz.b tcc__r0
+ lda.l loom_pvs_runtime_input_held,x
  sta.b tcc__r1
  eor #$ffff
  and.b tcc__r0
  ora.l loom_pvs_runtime_input_pressed,x
  sta.l loom_pvs_runtime_input_pressed,x
  lda.b tcc__r0
  eor #$ffff
  and.b tcc__r1
  ora.l loom_pvs_runtime_input_released,x
  sta.l loom_pvs_runtime_input_released,x
  lda.b tcc__r0
  sta.l loom_pvs_runtime_input_held,x
  rts

; void loom_pvs_dma_run(const LoomDmaJob *jobs,  ; 5,s / 7,s
;                       loom_u8 **sources,        ; 9,s / 11,s
;                       loom_u16 count)           ; 13,s
; The frame's staged transfers, in VBlank, on DMA channel 0: each job's
; registers written straight rather than through a C loop, a dispatcher and
; PVSnesLib's copy routine, which cost a few scanlines a job. A board move
; staged a dozen jobs and ran the copies past the end of VBlank, where the
; PPU drops VRAM writes: cells the rules had erased stayed on screen.
; LoomDmaJob (14 bytes): destination_offset 6, byte_count 8,
; destination_kind 11 (VRAM 0, CGRAM 1, VRAM column 2). A source is a
; four-byte pointer: offset word, bank word.
loom_pvs_dma_run:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r0
  lda 7,s
  sta.b tcc__r0h
  lda 9,s
  sta.b tcc__r1
  lda 11,s
  sta.b tcc__r1h
  lda 13,s
  sta.b tcc__r2                         ; jobs left
  bne +
  brl loom_pvs_dma_run_done
+
  stz.b tcc__r2h                        ; the job's byte offset
  stz.b tcc__r3                         ; the source's byte offset
loom_pvs_dma_run_job:
  ldy.b tcc__r2h
  iny
  iny
  iny
  iny
  iny
  iny
  lda [tcc__r0],y                       ; destination_offset
  lsr a
  sta.b tcc__r3h                        ; as a word address
  iny
  iny
  lda [tcc__r0],y                       ; byte_count
  sta.b tcc__r4
  iny
  iny
  iny
  lda [tcc__r0],y                       ; destination_kind (low byte)
  and #$00ff
  cmp #1
  beq loom_pvs_dma_run_cgram
  sep #$20
  cmp #2
  beq +
  lda #$80                              ; VRAM: a word at a time
  bra ++
+ lda #$81                              ; a column: 32 words a step
++ sta.l $002115
  rep #$20
  lda.b tcc__r3h
  sta.l $002116
  lda #$1801                            ; two registers from $2118, to B
  sta.l $004300
  bra loom_pvs_dma_run_source
loom_pvs_dma_run_cgram:
  sep #$20
  lda.b tcc__r3h
  sta.l $002121
  rep #$20
  lda #$2200                            ; one register, $2122
  sta.l $004300
loom_pvs_dma_run_source:
  ldy.b tcc__r3
  lda [tcc__r1],y
  sta.l $004302
  iny
  iny
  sep #$20
  lda [tcc__r1],y
  sta.l $004304
  rep #$20
  lda.b tcc__r4
  sta.l $004305
  sep #$20
  lda #$01
  sta.l $00420b
  rep #$20
  lda.b tcc__r2h
  clc
  adc #14
  sta.b tcc__r2h
  lda.b tcc__r3
  clc
  adc #4
  sta.b tcc__r3
  dec.b tcc__r2
  beq loom_pvs_dma_run_done
  brl loom_pvs_dma_run_job
loom_pvs_dma_run_done:
  plp
  rtl

.ENDS
