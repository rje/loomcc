.include "hdr.asm"

; loomcc testbed: loom_pvs_surface_flush (Loom e239e1c, unchanged at HEAD) with the helpers it calls (loom_pvs_flush_spec, loom_pvs_board_mul). The board's RAM section is kept whole.
; Extracted verbatim from Loom's runtime/backends/pvsneslib/src/board.asm;
; only unrelated routines are dropped and `.BASE LOOM_ROM_BASE` (a Loom
; project-header define, $80 for a LoROM FastROM cartridge) is written `.BASE $80`.

.accu 16
.index 16
.16bit

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
