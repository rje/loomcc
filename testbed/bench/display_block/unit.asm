.include "hdr.asm"

.accu 16
.index 16
.16bit

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
