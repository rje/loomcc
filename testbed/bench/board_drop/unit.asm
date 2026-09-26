.include "hdr.asm"

; loomcc testbed: loom_pvs_board_drop (Loom cb5a47f, unchanged at HEAD) with the helpers it calls: the multiply, the record load, the fit core, the paint core with its cell draw, kept-hash update and dirty-span widening, and loom_pvs_board_move_from (a tail of loom_pvs_board_move).
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
loom_pvs_rep_held dsb 2
loom_pvs_rep_fire_at dsb 2
loom_pvs_rep_period dsb 2
loom_pvs_rep_fired dsb 2
loom_pvs_rep_bit dsb 2
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


; The paint on the loaded record for shape, x, y and kind in RAM. Keeps
; r0/r0h (the record).
loom_pvs_board_paint_core:
  lda #0
  sta.l loom_pvs_board_row
loom_pvs_board_paint_row:
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
loom_pvs_board_paint_col:
  lda.l loom_pvs_board_shape
  lsr a
  sta.l loom_pvs_board_shape
  bcs +
  brl loom_pvs_board_paint_next
+ lda.l loom_pvs_board_x
  clc
  adc.l loom_pvs_board_col
  bpl +
  brl loom_pvs_board_paint_next
+ sta.l loom_pvs_board_cx
  cmp.l loom_pvs_board_width
  bcc +
  brl loom_pvs_board_paint_next
+ lda.l loom_pvs_board_cy
  bpl +
  brl loom_pvs_board_paint_next
+ cmp.l loom_pvs_board_height
  bcc +
  brl loom_pvs_board_paint_next
+ lda.l loom_pvs_board_rowbase
  clc
  adc.l loom_pvs_board_cx
  sta.l loom_pvs_board_idx
  tay
  sep #$20
  lda [tcc__r9],y
  cmp.l loom_pvs_board_kind
  bne +
  rep #$20
  brl loom_pvs_board_paint_next        ; already this kind
+ rep #$20
  and #$00ff
  sta.l loom_pvs_board_old
  sep #$20
  lda.l loom_pvs_board_kind
  sta [tcc__r9],y
  rep #$20
  jsr loom_pvs_board_rehash
  jsr loom_pvs_board_draw
loom_pvs_board_paint_next:
  lda.l loom_pvs_board_col
  inc a
  sta.l loom_pvs_board_col
  cmp #4
  bcs +
  brl loom_pvs_board_paint_col
+ lda.l loom_pvs_board_row
  inc a
  sta.l loom_pvs_board_row
  cmp #4
  bcs +
  brl loom_pvs_board_paint_row
+ rts

; Draws cell (cx, cy) as `kind`: cell_size rows of cell_size words from
; kind_words into the shadow, each row's dirty span widened.
loom_pvs_board_draw:
  lda.l loom_pvs_board_cx
  ldx.w #0
  tax
  lda.l loom_pvs_board_cs
  jsr loom_pvs_board_mul
  sta.l loom_pvs_board_tx               ; tx = cx * cs
  lda.l loom_pvs_board_tx
  clc
  adc.l loom_pvs_board_cs
  dec a
  sta.l loom_pvs_board_end              ; end = tx + cs - 1
  lda #0
  sta.l loom_pvs_board_sub
loom_pvs_board_draw_sub:
  ; ty = cy * cs + sub
  lda.l loom_pvs_board_cy
  tax
  lda.l loom_pvs_board_cs
  jsr loom_pvs_board_mul
  clc
  adc.l loom_pvs_board_sub
  sta.l loom_pvs_board_ty
  ; woff = (kind * cs * cs + sub * cs) * 2
  lda.l loom_pvs_board_kind
  tax
  lda.l loom_pvs_board_cs
  jsr loom_pvs_board_mul
  tax
  lda.l loom_pvs_board_cs
  jsr loom_pvs_board_mul                ; kind * cs * cs
  sta.l loom_pvs_board_tmp
  lda.l loom_pvs_board_sub
  tax
  lda.l loom_pvs_board_cs
  jsr loom_pvs_board_mul                ; sub * cs
  clc
  adc.l loom_pvs_board_tmp
  asl a
  sta.l loom_pvs_board_woff
  ; soff = (ty * stride + tx) * 2
  lda.l loom_pvs_board_ty
  tax
  lda.l loom_pvs_board_stride
  jsr loom_pvs_board_mul
  clc
  adc.l loom_pvs_board_tx
  asl a
  sta.l loom_pvs_board_tmp
  ; cell_size words
  ldx.w #0
- phx
  txa
  asl a
  clc
  adc.l loom_pvs_board_woff
  tay
  lda [tcc__r5],y
  sta.b tcc__r1
  txa
  asl a
  clc
  adc.l loom_pvs_board_tmp
  tay
  lda.b tcc__r1
  sta [tcc__r10],y
  plx
  inx
  txa
  cmp.l loom_pvs_board_cs
  bcc -
  jsr loom_pvs_board_dirty
  lda.l loom_pvs_board_sub
  inc a
  sta.l loom_pvs_board_sub
  cmp.l loom_pvs_board_cs
  bcs +
  brl loom_pvs_board_draw_sub
+ rts

; The kept hash moves by (kind - old) * (2 * idx + 1) when it is valid.
loom_pvs_board_rehash:
  ldy #34
  lda [tcc__r0],y
  bne +
  rts
+ lda.l loom_pvs_board_idx
  asl a
  inc a
  sta.b tcc__r1                         ; the cell's weight
  ldy #32
  lda [tcc__r0],y
  sta.b tcc__r2                         ; the hash
  lda.l loom_pvs_board_old
  tax
  beq loom_pvs_board_rehash_add
- lda.b tcc__r2
  sec
  sbc.b tcc__r1
  sta.b tcc__r2
  dex
  bne -
loom_pvs_board_rehash_add:
  lda.l loom_pvs_board_kind
  and #$00ff
  tax
  beq loom_pvs_board_rehash_done
- lda.b tcc__r2
  clc
  adc.b tcc__r1
  sta.b tcc__r2
  dex
  bne -
loom_pvs_board_rehash_done:
  lda.b tcc__r2
  ldy #32
  sta [tcc__r0],y
  rts

; Widens tile row ty's dirty span to tx .. end, counting new cells into
; *pending: loom_surface_dirty.
loom_pvs_board_dirty:
  lda.l loom_pvs_board_ty
  tay
  sep #$20
  lda [tcc__f2],y
  cmp #$ff
  bne loom_pvs_board_dirty_widen
  lda.l loom_pvs_board_tx
  sta [tcc__f2],y
  lda.l loom_pvs_board_end
  sta [tcc__f3],y
  rep #$20
  lda [tcc__r4]
  clc
  adc.l loom_pvs_board_cs
  sta [tcc__r4]
  rts
loom_pvs_board_dirty_widen:
  rep #$20
  and #$00ff
  sta.b tcc__r1                         ; first
  lda.l loom_pvs_board_tx
  cmp.b tcc__r1
  bcs +
  lda.b tcc__r1
  sec
  sbc.l loom_pvs_board_tx
  clc
  adc [tcc__r4]
  sta [tcc__r4]
  sep #$20
  lda.l loom_pvs_board_tx
  sta [tcc__f2],y
  rep #$20
+ sep #$20
  lda [tcc__f3],y
  rep #$20
  and #$00ff
  sta.b tcc__r1                         ; last
  lda.l loom_pvs_board_end
  cmp.b tcc__r1
  beq +
  bcc +
  sec
  sbc.b tcc__r1
  clc
  adc [tcc__r4]
  sta [tcc__r4]
  sep #$20
  lda.l loom_pvs_board_end
  sta [tcc__f3],y
  rep #$20
+ rts

loom_pvs_board_move_from:
  lda.l loom_pvs_board_mv
  sta.l loom_pvs_board_shape
  lda.l loom_pvs_board_mv+2
  sta.l loom_pvs_board_x
  lda.l loom_pvs_board_mv+4
  sta.l loom_pvs_board_y
  rts

; loom_u16 loom_pvs_board_drop(const LoomBoardPaint *paint, ; 5,s / 7,s
;                              loom_u16 shape,              ; 9,s
;                              loom_s16 x,                  ; 11,s
;                              loom_s16 y,                  ; 13,s
;                              loom_u16 kind)               ; 15,s
; A hard drop in one call: lifts the shape off, finds the lowest row it
; fits at straight below, and stamps it there. r0 <- the rows it fell.
loom_pvs_board_drop:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r0
  lda 7,s
  sta.b tcc__r0h
  lda 9,s
  sta.l loom_pvs_board_mv
  lda 11,s
  sta.l loom_pvs_board_mv+2
  lda 13,s
  sta.l loom_pvs_board_mv+4
  lda 15,s
  and #$00ff
  sta.l loom_pvs_board_mv+12
  jsr loom_pvs_board_load
  jsr loom_pvs_board_move_from
  lda #0
  sta.l loom_pvs_board_kind
  jsr loom_pvs_board_paint_core         ; lift it off
  lda #0
  sta.l loom_pvs_board_mv+14            ; rows fallen
- jsr loom_pvs_board_move_from
  lda.l loom_pvs_board_mv+4
  inc a
  sta.l loom_pvs_board_y
  jsr loom_pvs_board_fits_core
  beq +
  lda.l loom_pvs_board_mv+4
  inc a
  sta.l loom_pvs_board_mv+4
  lda.l loom_pvs_board_mv+14
  inc a
  sta.l loom_pvs_board_mv+14
  bra -
+ jsr loom_pvs_board_move_from
  lda.l loom_pvs_board_mv+12
  sta.l loom_pvs_board_kind
  jsr loom_pvs_board_paint_core         ; where it lands
  lda.l loom_pvs_board_mv+14
  sta.b tcc__r0
  plp
  rtl

.ENDS
