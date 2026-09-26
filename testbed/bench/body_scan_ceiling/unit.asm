.include "hdr.asm"

.accu 16
.index 16
.16bit

; The platformer bodies' tile probes, in assembly: the wall probe along X,
; the floor scan under the feet and the ceiling scan over the head. The
; player's body and every actor's body run all three every tick, and
; through 816-tcc the three loops cost the player about 45 scanlines of
; the frame's 262. The portable C renditions stay in runtime/src/movement.c
; and runtime/src/actor.c under `#else`, exercised by the host suites; the
; two must agree exactly, and the tick-clock ROM tests pin the positions
; that hold them together.
;
; All three read the active grid through loom_movement_grid, a C struct in
; movement.h whose layout is spelled out here: cells (four-byte pointer)
; at 0, pixel_width (s16) at 4, pixel_height (s16) at 6, row_offsets (u16
; per tile row) from 8. movement.c asserts the sizes; a field moved there
; without this file following makes every lookup read the wrong cell.
;
; 816-tcc ABI: JSL/RTL, the first argument at 4,s (5,s after our php),
; 16-bit results in tcc__r0. Direct page is zero, so tcc__r0..r5 and
; tcc__r9/r10 are scratch; nothing of the caller's lives in them.
;
; Cells: 0 none, 1 solid, 2 one-way, 3 slope up right, 4 slope up left.

.SECTION "loom.pvs.body.code" SUPERFREE KEEP

; Signed 16-bit compare, A against the operand: leaves N set when A is
; less, as the C's signed compare would. Clobbers A.
.MACRO LOOM_SIGNED_CMP ARGS operand
  sec
  sbc.b operand
  bvc _skip\@
  eor #$8000
_skip\@:
.ENDM

; The cell at (X, A): X the pixel column, A the pixel row. Returns the cell
; in A (16-bit, 0..4), solid outside the room. Clobbers X, Y, tcc__r0,
; tcc__r10; the cells pointer must already sit in tcc__r9/tcc__r9h.
loom_pvs_body_cell:
  rep #$30
  bmi loom_pvs_body_cell_solid          ; y < 0
  cmp.l loom_movement_grid+6
  bcs loom_pvs_body_cell_solid          ; y >= pixel_height
  sta.b tcc__r0
  txa
  bmi loom_pvs_body_cell_solid          ; x < 0
  cmp.l loom_movement_grid+4
  bcs loom_pvs_body_cell_solid          ; x >= pixel_width
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r10                        ; x >> 4
  lda.b tcc__r0
  lsr a
  lsr a
  lsr a
  and #$fffe                            ; (y >> 4) * 2
  tax
  lda.l loom_movement_grid+8,x          ; row_offsets[y >> 4]
  clc
  adc.b tcc__r10
  tay
  sep #$20
  lda [tcc__r9],y
  rep #$20
  and #$00ff
  rts
loom_pvs_body_cell_solid:
  rep #$30
  lda #1
  rts

; Loads the grid's cells pointer into tcc__r9/tcc__r9h.
loom_pvs_body_grid:
  rep #$30
  lda.l loom_movement_grid
  sta.b tcc__r9
  lda.l loom_movement_grid+2
  sta.b tcc__r9h
  rts

; loom_s16 loom_pvs_body_scan_ceiling(loom_s16 left,  ;  5,s
;                                     loom_s16 right, ;  7,s
;                                     loom_s16 head,  ;  9,s
;                                     loom_s16 reach) ; 11,s
; The row under the first solid cell between the head and the reach
; above it (row_top + 16), or -1.
; Scratch: r1 row_top, r2 the column, r4 right, r5 reach.
loom_pvs_body_scan_ceiling:
  php
  rep #$30
  jsr loom_pvs_body_grid
  lda 7,s
  sta.b tcc__r4
  lda 11,s
  sta.b tcc__r5
  lda 9,s
  dec a
  and #$fff0
  sta.b tcc__r1                         ; row_top = (head - 1) & ~15
loom_pvs_body_scan_ceiling_row:
  lda.b tcc__r1
  clc
  adc #15
  LOOM_SIGNED_CMP tcc__r5
  bpl +
  brl loom_pvs_body_scan_ceiling_none   ; row_top + 15 < reach
+
  lda 5,s
  sta.b tcc__r2
loom_pvs_body_scan_ceiling_column:
  lda.b tcc__r2
  LOOM_SIGNED_CMP tcc__r4
  beq +
  bmi +
  brl loom_pvs_body_scan_ceiling_next
+ ldx.b tcc__r2
  lda.b tcc__r1
  jsr loom_pvs_body_cell
  cmp #1
  beq loom_pvs_body_scan_ceiling_hit
  lda.b tcc__r2
  ora #$000f
  inc a
  sta.b tcc__r2
  bra loom_pvs_body_scan_ceiling_column
loom_pvs_body_scan_ceiling_next:
  lda.b tcc__r1
  sec
  sbc #16
  sta.b tcc__r1
  bra loom_pvs_body_scan_ceiling_row
loom_pvs_body_scan_ceiling_hit:
  lda.b tcc__r1
  clc
  adc #16
  sta.b tcc__r0
  plp
  rtl
loom_pvs_body_scan_ceiling_none:
  lda #$ffff
  sta.b tcc__r0
  plp
  rtl

.ENDS
