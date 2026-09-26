.include "hdr.asm"

; loomcc testbed: loom_pvs_board_fits (Loom e239e1c, unchanged at HEAD) with the multiply and record-load helpers it calls.
; Extracted verbatim from Loom's runtime/backends/pvsneslib/src/board.asm;
; only unrelated routines are dropped and `.BASE LOOM_ROM_BASE` (a Loom
; project-header define, $80 for a LoROM FastROM cartridge) is written `.BASE $80`.

.accu 16
.index 16
.16bit

; The board's per-move primitives in assembly (PERF-004): the fit test, the
; shape paint that writes a piece's cells, their tilemap words and the
; surface's dirty spans in one pass, and the cell hash the debug witness
; publishes. Through 816-tcc a falling-piece game's erase, fit test and
; stamp cost about 390 scanlines a step -- a board lookup, a multiply
; helper and two surface calls per cell -- and the hash 270 a tick. The C
; renditions stay in runtime/src/board.c for the host suites; the two must
; agree cell for cell, and Stack's ROM test pins the hash.
;
; Nothing here names a C symbol: board.c hands every routine a
; LoomBoardPaint record (board.h; board.c asserts its 36 bytes), so the unit
; links into every project whether or not it has a board:
;   cells 0, kind_words 4, shadow 8, first 12, last 16, pending 20 (four-byte
;   pointers), width 24, height 26, cell_size 28, stride 30, hash 32 and
;   hash_valid 34 (words).
; `shadow` is the surface's first word, `first`/`last` its per-row dirty
; span bytes (0xff first means clean), `pending` the surface's count of
; dirty cells, `stride` its width in tiles.
;
; 816-tcc ABI: JSL/RTL, the first argument at 5,s after our php, 16-bit
; results in tcc__r0; tcc__r0..r5, r9, r10, f2 and f3 are scratch.

.BASE $00
.RAMSECTION "loom.pvs.board.state" BANK $7e SLOT 2
loom_pvs_board_shape dsb 2
loom_pvs_board_x dsb 2
loom_pvs_board_y dsb 2
loom_pvs_board_kind dsb 2
loom_pvs_board_width dsb 2
loom_pvs_board_height dsb 2
loom_pvs_board_cs dsb 2
loom_pvs_board_stride dsb 2
loom_pvs_board_row dsb 2
loom_pvs_board_col dsb 2
loom_pvs_board_cx dsb 2
loom_pvs_board_cy dsb 2
loom_pvs_board_sub dsb 2
loom_pvs_board_tx dsb 2
loom_pvs_board_ty dsb 2
loom_pvs_board_end dsb 2
loom_pvs_board_woff dsb 2
loom_pvs_board_mul_a dsb 2
loom_pvs_board_tmp dsb 2
loom_pvs_board_rowbase dsb 2
loom_pvs_board_mv dsb 16
loom_pvs_flush_rows dsb 2
loom_pvs_clear_read dsb 2
loom_pvs_clear_write dsb 2
loom_pvs_clear_count dsb 2
loom_pvs_clear_lowest dsb 2
loom_pvs_flush_bytes dsb 2
loom_pvs_flush_jobs dsb 2
loom_pvs_flush_surface dsb 2
loom_pvs_flush_row dsb 2
loom_pvs_flush_height dsb 2
loom_pvs_flush_width dsb 2
loom_pvs_flush_first dsb 2
loom_pvs_flush_last dsb 2
loom_pvs_flush_words dsb 2
loom_pvs_flush_room dsb 2
loom_pvs_flush_rowoff dsb 2
loom_pvs_board_old dsb 2
loom_pvs_board_idx dsb 2
.ENDS
.BASE $80

.SECTION "loom.pvs.board.code" SUPERFREE KEEP

; A <- A * X, both under 256, on the CPU's multiplier ($4202 x $4203 ->
; $4216 eight cycles later). Nothing else in the cartridge uses it -- not
; PVSnesLib's libraries, not Loom's NMI -- so no interrupt can clobber a
; product in flight. Clobbers nothing else.
loom_pvs_board_mul:
  sep #$20
  sta.l $004202
  txa
  sta.l $004203
  rep #$20
  nop
  nop
  lda.l $004216
  rts

; Reads the record at r0/r0h into the scratch: cells to r9, kind_words to
; r5, shadow to r10, first to f2, last to f3, pending to r4, the sizes to RAM.
loom_pvs_board_load:
  ldy #0
  lda [tcc__r0],y
  sta.b tcc__r9
  ldy #2
  lda [tcc__r0],y
  sta.b tcc__r9h
  ldy #4
  lda [tcc__r0],y
  sta.b tcc__r5
  ldy #6
  lda [tcc__r0],y
  sta.b tcc__r5h
  ldy #8
  lda [tcc__r0],y
  sta.b tcc__r10
  ldy #10
  lda [tcc__r0],y
  sta.b tcc__r10h
  ldy #12
  lda [tcc__r0],y
  sta.b tcc__f2
  ldy #14
  lda [tcc__r0],y
  sta.b tcc__f2h
  ldy #16
  lda [tcc__r0],y
  sta.b tcc__f3
  ldy #18
  lda [tcc__r0],y
  sta.b tcc__f3h
  ldy #20
  lda [tcc__r0],y
  sta.b tcc__r4
  ldy #22
  lda [tcc__r0],y
  sta.b tcc__r4h
  ldy #24
  lda [tcc__r0],y
  sta.l loom_pvs_board_width
  ldy #26
  lda [tcc__r0],y
  sta.l loom_pvs_board_height
  ldy #28
  lda [tcc__r0],y
  sta.l loom_pvs_board_cs
  ldy #30
  lda [tcc__r0],y
  sta.l loom_pvs_board_stride
  rts

; loom_u16 loom_pvs_board_fits(const LoomBoardPaint *paint, ; 5,s / 7,s
;                              loom_u16 shape,              ; 9,s
;                              loom_s16 x,                  ; 11,s
;                              loom_s16 y)                  ; 13,s
; 1 when every cell of the 4x4 shape (bit row * 4 + column) at x, y is on
; the board and empty; rows above the board are open.
loom_pvs_board_fits:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r0
  lda 7,s
  sta.b tcc__r0h
  lda 9,s
  sta.l loom_pvs_board_shape
  lda 11,s
  sta.l loom_pvs_board_x
  lda 13,s
  sta.l loom_pvs_board_y
  jsr loom_pvs_board_load
  jsr loom_pvs_board_fits_core
  sta.b tcc__r0
  plp
  rtl

; The fit test on the loaded record for shape, x and y in RAM: A <- 1 or 0.
; Keeps r0/r0h (the record).
loom_pvs_board_fits_core:
  lda #0
  sta.l loom_pvs_board_row
loom_pvs_board_fits_row:
  lda.l loom_pvs_board_y
  clc
  adc.l loom_pvs_board_row
  sta.l loom_pvs_board_cy
  bmi +
  tax
  lda.l loom_pvs_board_width
  jsr loom_pvs_board_mul
  sta.l loom_pvs_board_rowbase          ; cy * width, once a row
+
  lda #0
  sta.l loom_pvs_board_col
loom_pvs_board_fits_col:
  lda.l loom_pvs_board_shape
  lsr a
  sta.l loom_pvs_board_shape
  bcs +
  brl loom_pvs_board_fits_next
+ lda.l loom_pvs_board_x
  clc
  adc.l loom_pvs_board_col
  bpl +
  brl loom_pvs_board_fits_no          ; left of the board
+ sta.l loom_pvs_board_cx
  cmp.l loom_pvs_board_width
  bcc +
  brl loom_pvs_board_fits_no          ; right of the board
+ lda.l loom_pvs_board_cy
  bpl +
  brl loom_pvs_board_fits_next        ; above the board: open
+ cmp.l loom_pvs_board_height
  bcc +
  brl loom_pvs_board_fits_no          ; below the board
+ lda.l loom_pvs_board_rowbase
  clc
  adc.l loom_pvs_board_cx
  tay
  sep #$20
  lda [tcc__r9],y
  rep #$20
  and #$00ff
  beq loom_pvs_board_fits_next
  brl loom_pvs_board_fits_no
loom_pvs_board_fits_next:
  lda.l loom_pvs_board_col
  inc a
  sta.l loom_pvs_board_col
  cmp #4
  bcs +
  brl loom_pvs_board_fits_col
+ lda.l loom_pvs_board_row
  inc a
  sta.l loom_pvs_board_row
  cmp #4
  bcs +
  brl loom_pvs_board_fits_row
+ lda #1
  rts
loom_pvs_board_fits_no:
  lda #0
  rts

.ENDS
