.include "hdr.asm"

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
.BASE LOOM_ROM_BASE

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

; void loom_pvs_board_paint(const LoomBoardPaint *paint, ; 5,s / 7,s
;                           loom_u16 shape,              ; 9,s
;                           loom_s16 x,                  ; 11,s
;                           loom_s16 y,                  ; 13,s
;                           loom_u16 kind)               ; 15,s
; Writes `kind` into every cell of the shape that is on the board and
; holds something else, with its tilemap words in the surface shadow and
; the row's dirty span widened: loom_board_set for each, in one pass.
loom_pvs_board_paint:
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
  lda 15,s
  and #$00ff
  sta.l loom_pvs_board_kind
  jsr loom_pvs_board_load
  jsr loom_pvs_board_paint_core
  plp
  rtl

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

; loom_u16 loom_pvs_board_move(const LoomBoardPaint *paint, ; 5,s / 7,s
;                              loom_u16 from_shape,         ; 9,s
;                              loom_s16 from_x,             ; 11,s
;                              loom_s16 from_y,             ; 13,s
;                              loom_u16 to_shape,           ; 15,s
;                              loom_s16 to_x,               ; 17,s
;                              loom_s16 to_y,               ; 19,s
;                              loom_u16 kind)               ; 21,s
; A piece's move in one call: lifts it off, and paints it where it goes if
; that fits, or back where it was. 1 when it moved. The cells, words, dirty
; spans and kept hash end as erase, fits and stamp would leave them.
loom_pvs_board_move:
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
  sta.l loom_pvs_board_mv+6
  lda 17,s
  sta.l loom_pvs_board_mv+8
  lda 19,s
  sta.l loom_pvs_board_mv+10
  lda 21,s
  and #$00ff
  sta.l loom_pvs_board_mv+12
  jsr loom_pvs_board_load
  jsr loom_pvs_board_move_from
  lda #0
  sta.l loom_pvs_board_kind
  jsr loom_pvs_board_paint_core         ; lift it off
  lda.l loom_pvs_board_mv+6
  sta.l loom_pvs_board_shape
  lda.l loom_pvs_board_mv+8
  sta.l loom_pvs_board_x
  lda.l loom_pvs_board_mv+10
  sta.l loom_pvs_board_y
  jsr loom_pvs_board_fits_core
  sta.l loom_pvs_board_mv+14           ; the draw uses _tmp
  bne +
  jsr loom_pvs_board_move_from          ; back where it was
  bra ++
+ lda.l loom_pvs_board_mv+6
  sta.l loom_pvs_board_shape
  lda.l loom_pvs_board_mv+8
  sta.l loom_pvs_board_x
  lda.l loom_pvs_board_mv+10
  sta.l loom_pvs_board_y
++ lda.l loom_pvs_board_mv+12
  sta.l loom_pvs_board_kind
  jsr loom_pvs_board_paint_core
  lda.l loom_pvs_board_mv+14
  sta.b tcc__r0
  plp
  rtl
loom_pvs_board_move_from:
  lda.l loom_pvs_board_mv
  sta.l loom_pvs_board_shape
  lda.l loom_pvs_board_mv+2
  sta.l loom_pvs_board_x
  lda.l loom_pvs_board_mv+4
  sta.l loom_pvs_board_y
  rts

; loom_u16 loom_pvs_board_clear_full_rows(const LoomBoardPaint *paint) ; 5,s / 7,s
; Removes every full row, drops the rows above into their places, empties
; the top, and redraws rows 0 through the lowest cleared one: the count
; cleared. The kept hash is marked stale when anything changed.
loom_pvs_board_clear_full_rows:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r0
  lda 7,s
  sta.b tcc__r0h
  jsr loom_pvs_board_load
  lda #0
  sta.l loom_pvs_clear_count
  sta.l loom_pvs_clear_lowest
  lda.l loom_pvs_board_height
  dec a
  sta.l loom_pvs_clear_read
  sta.l loom_pvs_clear_write
loom_pvs_clear_row:
  ; is row `read` full?
  lda.l loom_pvs_clear_read
  tax
  lda.l loom_pvs_board_width
  jsr loom_pvs_board_mul
  sta.b tcc__r1                         ; read row offset
  tay
  ldx.w #0
  sep #$20
- lda [tcc__r9],y
  beq loom_pvs_clear_keep
  iny
  inx
  rep #$20
  txa
  cmp.l loom_pvs_board_width
  sep #$20
  bcc -
  rep #$20
  ; full: count it, remember the lowest
  lda.l loom_pvs_clear_count
  bne +
  lda.l loom_pvs_clear_read
  sta.l loom_pvs_clear_lowest
+ lda.l loom_pvs_clear_count
  inc a
  sta.l loom_pvs_clear_count
  brl loom_pvs_clear_step
loom_pvs_clear_keep:
  rep #$20
  lda.l loom_pvs_clear_write
  cmp.l loom_pvs_clear_read
  beq +
  ; copy row read down to row write
  tax
  lda.l loom_pvs_board_width
  jsr loom_pvs_board_mul
  sta.b tcc__r2                         ; write row offset
  ldx.w #0
- txa
  clc
  adc.b tcc__r1
  tay
  sep #$20
  lda [tcc__r9],y
  sta.b tcc__r3
  rep #$20
  txa
  clc
  adc.b tcc__r2
  tay
  sep #$20
  lda.b tcc__r3
  sta [tcc__r9],y
  rep #$20
  inx
  txa
  cmp.l loom_pvs_board_width
  bcc -
+ lda.l loom_pvs_clear_write
  dec a
  sta.l loom_pvs_clear_write
  bmi loom_pvs_clear_scanned
loom_pvs_clear_step:
  lda.l loom_pvs_clear_read
  dec a
  sta.l loom_pvs_clear_read
  bmi loom_pvs_clear_scanned
  brl loom_pvs_clear_row
loom_pvs_clear_scanned:
  lda.l loom_pvs_clear_count
  bne +
  brl loom_pvs_clear_exit
+ ; the rows that dropped in from above are empty
  lda.l loom_pvs_clear_count
  tax
  lda.l loom_pvs_board_width
  jsr loom_pvs_board_mul
  sta.b tcc__r1
  ldy.w #0
  sep #$20
- lda #0
  sta [tcc__r9],y
  iny
  rep #$20
  tya
  cmp.b tcc__r1
  sep #$20
  bcc -
  rep #$20
  ; redraw rows 0 .. lowest, cell by cell
  lda #0
  ldy #34
  sta [tcc__r0],y                       ; the kept hash is stale
  sta.l loom_pvs_board_cy
loom_pvs_clear_redraw_row:
  lda #0
  sta.l loom_pvs_board_cx
loom_pvs_clear_redraw_cell:
  lda.l loom_pvs_board_cy
  tax
  lda.l loom_pvs_board_width
  jsr loom_pvs_board_mul
  clc
  adc.l loom_pvs_board_cx
  tay
  sep #$20
  lda [tcc__r9],y
  rep #$20
  and #$00ff
  sta.l loom_pvs_board_kind
  jsr loom_pvs_board_draw
  lda.l loom_pvs_board_cx
  inc a
  sta.l loom_pvs_board_cx
  cmp.l loom_pvs_board_width
  bcc loom_pvs_clear_redraw_cell
  lda.l loom_pvs_board_cy
  inc a
  sta.l loom_pvs_board_cy
  dec a
  cmp.l loom_pvs_clear_lowest
  bcc loom_pvs_clear_redraw_row
loom_pvs_clear_exit:
  lda.l loom_pvs_clear_count
  sta.b tcc__r0
  plp
  rtl

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

; void loom_pvs_board_fill(const LoomBoardPaint *paint, ; 5,s / 7,s
;                          loom_u16 kind)               ; 9,s
; Every cell becomes `kind`, and each that changed is redrawn:
; loom_board_clear's wipe, without a C statement per cell, and nothing at
; all for a board that is already clear. The kept hash is marked stale.
loom_pvs_board_fill:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r0
  lda 7,s
  sta.b tcc__r0h
  lda 9,s
  and #$00ff
  sta.l loom_pvs_board_kind
  jsr loom_pvs_board_load
  lda #0
  ldy #34
  sta [tcc__r0],y
  sta.l loom_pvs_board_cy
loom_pvs_fill_row:
  lda #0
  sta.l loom_pvs_board_cx
loom_pvs_fill_cell:
  lda.l loom_pvs_board_cy
  tax
  lda.l loom_pvs_board_width
  jsr loom_pvs_board_mul
  clc
  adc.l loom_pvs_board_cx
  tay
  sep #$20
  lda [tcc__r9],y
  cmp.l loom_pvs_board_kind
  beq +                                 ; already this kind: nothing to draw
  lda.l loom_pvs_board_kind
  sta [tcc__r9],y
  rep #$20
  jsr loom_pvs_board_draw
+ rep #$20
  lda.l loom_pvs_board_cx
  inc a
  sta.l loom_pvs_board_cx
  cmp.l loom_pvs_board_width
  bcc loom_pvs_fill_cell
  lda.l loom_pvs_board_cy
  inc a
  sta.l loom_pvs_board_cy
  cmp.l loom_pvs_board_height
  bcc loom_pvs_fill_row
  plp
  rtl

; loom_u16 loom_pvs_board_repeat(loom_u16 *held_ticks,   ; 5,s / 7,s
;                                loom_u8 *repeat_left,   ; 9,s / 11,s
;                                loom_u16 held,          ; 13,s
;                                loom_u16 delay,         ; 15,s
;                                loom_u16 period)        ; 17,s
; One board's key repeat for left, right, down and up (in that order, the
; bits 0x0200, 0x0100, 0x0400, 0x0800): a held direction fires on its first
; tick, again at tick delay + 2, then every period ticks. r0 <- the fired
; directions. The counters are the board's four u16 ticks and four u8
; countdowns.
loom_pvs_board_repeat:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r9
  lda 7,s
  sta.b tcc__r9h
  lda 9,s
  sta.b tcc__r10
  lda 11,s
  sta.b tcc__r10h
  lda 13,s
  sta.l loom_pvs_rep_held
  lda 15,s
  and #$00ff
  clc
  adc #2
  sta.l loom_pvs_rep_fire_at
  lda 17,s
  and #$00ff
  sta.l loom_pvs_rep_period
  lda #0
  sta.l loom_pvs_rep_fired
  ldx #0                                ; the direction
loom_pvs_rep_dir:
  lda.l loom_pvs_rep_bits,x
  sta.l loom_pvs_rep_bit
  txy                                   ; its ticks' offset (X is direction * 2)
  lda.l loom_pvs_rep_held
  and.l loom_pvs_rep_bit
  bne +
  lda #0
  sta [tcc__r9],y                       ; released
  brl loom_pvs_rep_next
+ lda [tcc__r9],y
  cmp #$ffff
  beq +
  inc a
  sta [tcc__r9],y
+ cmp #1
  beq loom_pvs_rep_fire
  cmp.l loom_pvs_rep_fire_at
  bcc loom_pvs_rep_next
  beq loom_pvs_rep_rearm
  ; past the delay: count down
  txa
  lsr a
  tay                                   ; its countdown's offset
  sep #$20
  lda [tcc__r10],y
  dec a
  sta [tcc__r10],y
  rep #$20
  and #$00ff
  bne loom_pvs_rep_next
loom_pvs_rep_rearm:
  txa
  lsr a
  tay
  sep #$20
  lda.l loom_pvs_rep_period
  sta [tcc__r10],y
  rep #$20
loom_pvs_rep_fire:
  lda.l loom_pvs_rep_fired
  ora.l loom_pvs_rep_bit
  sta.l loom_pvs_rep_fired
loom_pvs_rep_next:
  inx
  inx
  cpx #8
  bcs +
  brl loom_pvs_rep_dir
+ lda.l loom_pvs_rep_fired
  sta.b tcc__r0
  plp
  rtl
loom_pvs_rep_bits:
  .dw $0200, $0100, $0400, $0800

; loom_u16 loom_pvs_board_hash(const loom_u8 *cells, ; 5,s / 7,s
;                              loom_u16 count)       ; 9,s
; Every cell weighted by its odd position: the sum of cell * (2 * i + 1).
loom_pvs_board_hash:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r9
  lda 7,s
  sta.b tcc__r9h
  lda 9,s
  sta.b tcc__r1                         ; count
  lda #0
  sta.b tcc__r0
  lda #1
  sta.b tcc__r2                         ; the weight
  ldy.w #0
  cpy.b tcc__r1
  beq ++++
- sep #$20
  lda [tcc__r9],y
  rep #$20
  and #$00ff
  tax
  lda.b tcc__r0
  cpx.w #0
  beq +
-- clc
  adc.b tcc__r2
  dex
  bne --
+ sta.b tcc__r0
  lda.b tcc__r2
  clc
  adc #2
  sta.b tcc__r2
  iny
  cpy.b tcc__r1
  bne -
++++ plp
  rtl

; ---------------------------------------------------------------------------
; The surface flush (PERF-004): loom_surface_build_frame's walk over the
; dirty rows from the cursor, one VRAM job per dirty run within the tick's
; job and byte budget, written straight into the frame build's job list.
; surface.c fills a LoomSurfaceFlush record (surface.h; asserted there):
;   specs 0, firsts 4, lasts 8, jobs 12 (four-byte pointers), job_room 16,
;   surface_count 18, jobs_per_tick 20, bytes_per_tick 22, pending 24,
;   cursor_surface 26, cursor_row 28, next_job_id 30, jobs_written 32,
;   bytes_written 34 (words).
; A LoomSurfaceSpec is 12 bytes: map_word_base 0, shadow_word_offset 2,
; width 6 and height 7 (bytes). A LoomDmaJob is 14: job_id 0,
; source_handle 2, source_offset 4, destination_offset 6, byte_count 8,
; source_kind 10, destination_kind 11, policy 12, reserved 13. The dirty
; span tables are LOOM_SURFACE_ROWS_MAX (32) bytes a surface.
;
; loom_u16 loom_pvs_surface_flush(LoomSurfaceFlush *flush): a LoomStatus.

; r10 <- the spec of loom_pvs_flush_surface; its width and height to RAM.
loom_pvs_flush_spec:
  lda.l loom_pvs_flush_surface
  ldx #12
  jsr loom_pvs_board_mul
  clc
  adc.b tcc__r9
  sta.b tcc__r10
  lda.b tcc__r9h
  sta.b tcc__r10h
  ldy #6
  lda [tcc__r10],y
  pha
  and #$00ff
  sta.l loom_pvs_flush_width
  pla
  xba
  and #$00ff
  sta.l loom_pvs_flush_height
  rts

loom_pvs_surface_flush:
  php
  rep #$30
  lda 5,s
  sta.b tcc__r0
  lda 7,s
  sta.b tcc__r0h
  ldy #0
  lda [tcc__r0],y
  sta.b tcc__r9
  ldy #2
  lda [tcc__r0],y
  sta.b tcc__r9h                        ; specs
  ldy #4
  lda [tcc__r0],y
  sta.b tcc__f2
  ldy #6
  lda [tcc__r0],y
  sta.b tcc__f2h                        ; firsts
  ldy #8
  lda [tcc__r0],y
  sta.b tcc__f3
  ldy #10
  lda [tcc__r0],y
  sta.b tcc__f3h                        ; lasts
  ldy #12
  lda [tcc__r0],y
  sta.b tcc__r5
  ldy #14
  lda [tcc__r0],y
  sta.b tcc__r5h                        ; the next job
  ldy #16
  lda [tcc__r0],y
  sta.l loom_pvs_flush_room
  lda #0
  sta.l loom_pvs_flush_bytes
  sta.l loom_pvs_flush_jobs
  sta.l loom_pvs_flush_rows
  ; rows_left: every surface's height
  sta.l loom_pvs_flush_surface
- lda.l loom_pvs_flush_surface
  ldy #18
  cmp [tcc__r0],y
  bcs +
  jsr loom_pvs_flush_spec
  lda.l loom_pvs_flush_rows
  clc
  adc.l loom_pvs_flush_height
  sta.l loom_pvs_flush_rows
  lda.l loom_pvs_flush_surface
  inc a
  sta.l loom_pvs_flush_surface
  bra -
+ ldy #26
  lda [tcc__r0],y
  sta.l loom_pvs_flush_surface
  ldy #28
  lda [tcc__r0],y
  sta.l loom_pvs_flush_row
  jsr loom_pvs_flush_spec
loom_pvs_flush_loop:
  lda.l loom_pvs_flush_rows
  bne +
  brl loom_pvs_flush_done
+ ldy #24
  lda [tcc__r0],y
  bne +
  brl loom_pvs_flush_done               ; nothing pending
+ lda.l loom_pvs_flush_surface
  asl a
  asl a
  asl a
  asl a
  asl a
  clc
  adc.l loom_pvs_flush_row
  sta.l loom_pvs_flush_rowoff           ; surface * 32 + row
  tay
  sep #$20
  lda [tcc__f2],y
  rep #$20
  and #$00ff
  cmp #$00ff
  bne +
  brl loom_pvs_flush_next               ; clean
+ sta.l loom_pvs_flush_first
  lda.l loom_pvs_flush_jobs
  ldy #20
  cmp [tcc__r0],y
  bcc +
  brl loom_pvs_flush_done               ; the tick's jobs are spent
+ lda.l loom_pvs_flush_room
  bne +
  lda #2                                ; LOOM_STATUS_CAPACITY
  brl loom_pvs_flush_exit
+ lda.l loom_pvs_flush_rowoff
  tay
  sep #$20
  lda [tcc__f3],y
  rep #$20
  and #$00ff
  sta.l loom_pvs_flush_last
  sec
  sbc.l loom_pvs_flush_first
  inc a
  sta.l loom_pvs_flush_words            ; the run
  asl a
  sta.b tcc__r1                         ; its bytes
  ldy #22
  lda [tcc__r0],y
  sec
  sbc.l loom_pvs_flush_bytes
  sta.b tcc__r2                         ; the budget left
  lda.b tcc__r1
  cmp.b tcc__r2
  beq +
  bcc +
  lda.b tcc__r2                         ; send what fits
  lsr a
  sta.l loom_pvs_flush_words
  bne ++
  brl loom_pvs_flush_done
++ asl a
  sta.b tcc__r1
+ ; the job, in place
  ldy #30
  lda [tcc__r0],y
  pha
  inc a
  sta [tcc__r0],y                       ; ++next_job_id
  pla
  and #$3fff
  ora #$4000
  sta [tcc__r5]                         ; job_id
  ldy #2
  lda #1
  sta [tcc__r5],y                       ; source_handle
  lda.l loom_pvs_flush_row
  ldx.w #0
  pha
  lda.l loom_pvs_flush_width
  tax
  pla
  jsr loom_pvs_board_mul                ; row * width
  ldy #2
  clc
  adc [tcc__r10],y                      ; + shadow_word_offset
  clc
  adc.l loom_pvs_flush_first
  asl a
  ldy #4
  sta [tcc__r5],y                       ; source_offset
  lda.l loom_pvs_flush_row
  asl a
  asl a
  asl a
  asl a
  asl a                                 ; row * 32
  clc
  adc [tcc__r10]                        ; + map_word_base
  clc
  adc.l loom_pvs_flush_first
  asl a
  ldy #6
  sta [tcc__r5],y                       ; destination_offset
  lda.b tcc__r1
  ldy #8
  sta [tcc__r5],y                       ; byte_count
  lda #$0001
  ldy #10
  sta [tcc__r5],y                       ; WRAM block to VRAM
  lda #0
  ldy #12
  sta [tcc__r5],y                       ; required, reserved
  lda.b tcc__r5
  clc
  adc #14
  sta.b tcc__r5
  lda.l loom_pvs_flush_room
  dec a
  sta.l loom_pvs_flush_room
  lda.l loom_pvs_flush_jobs
  inc a
  sta.l loom_pvs_flush_jobs
  lda.l loom_pvs_flush_bytes
  clc
  adc.b tcc__r1
  sta.l loom_pvs_flush_bytes
  ldy #24
  lda [tcc__r0],y
  sec
  sbc.l loom_pvs_flush_words
  sta [tcc__r0],y                       ; pending
  lda.l loom_pvs_flush_last
  sec
  sbc.l loom_pvs_flush_first
  inc a
  cmp.l loom_pvs_flush_words
  bne +
  lda.l loom_pvs_flush_rowoff
  tay
  sep #$20
  lda #$ff
  sta [tcc__f2],y                       ; the row is clean
  rep #$20
  bra loom_pvs_flush_next
+ lda.l loom_pvs_flush_rowoff           ; the budget is spent mid-row
  tay
  lda.l loom_pvs_flush_first
  clc
  adc.l loom_pvs_flush_words
  sep #$20
  sta [tcc__f2],y
  rep #$20
  brl loom_pvs_flush_done
loom_pvs_flush_next:
  lda.l loom_pvs_flush_row
  inc a
  sta.l loom_pvs_flush_row
  cmp.l loom_pvs_flush_height
  bcc +
  lda #0
  sta.l loom_pvs_flush_row
  lda.l loom_pvs_flush_surface
  inc a
  ldy #18
  cmp [tcc__r0],y
  bcc ++
  lda #0
++ sta.l loom_pvs_flush_surface
  jsr loom_pvs_flush_spec
+ lda.l loom_pvs_flush_rows
  dec a
  sta.l loom_pvs_flush_rows
  brl loom_pvs_flush_loop
loom_pvs_flush_done:
  lda #0
loom_pvs_flush_exit:
  pha
  lda.l loom_pvs_flush_surface
  ldy #26
  sta [tcc__r0],y
  lda.l loom_pvs_flush_row
  ldy #28
  sta [tcc__r0],y
  lda.l loom_pvs_flush_jobs
  ldy #32
  sta [tcc__r0],y
  lda.l loom_pvs_flush_bytes
  ldy #34
  sta [tcc__r0],y
  pla
  sta.b tcc__r0
  plp
  rtl

.ENDS
