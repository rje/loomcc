.include "hdr.asm"

; loomcc testbed: loom_pvs_board_hash (Loom e239e1c, unchanged at HEAD). The board's RAM section is dropped: the hash uses only the tcc scratch registers.
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


.BASE $80

.SECTION "loom.pvs.board.code" SUPERFREE KEEP

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

.ENDS
