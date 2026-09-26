.include "hdr.asm"

.accu 16
.index 16
.16bit

; loomcc measurement harness: `main` for one benchmark ROM.
;
; crt0 calls main with D = $0000, DBR = $7E, native mode, A/X/Y 16-bit, and
; PVSnesLib's NMI handler armed. The sequence is:
;
;   phase 1  bench_setup() has returned; NMI and IRQ are off ($4200 = 0)
;            and $4201 bit 7 is set so a $2137 read latches the counters.
;   phase 2  a new VBlank has started twice (so the setup's frames are done);
;            latch H/V into bench_h0/bench_v0, jsl bench_run, latch H/V into
;            bench_h1/bench_v1 -- nothing else runs between the latches.
;   phase 3  bench_run has returned; bench_wrap = $80 when line 224 came
;            round during it (a V-IRQ at VTIME 224, masked by sei, sets
;            TIMEUP $4211 bit 7 and it stays set until read; RDNMI would not
;            do: its flag clears when VBlank ends). Then bench_run took about
;            a frame or longer and the H/V difference is useless.
;            Wait two more VBlank starts.
;   phase 4  bench_check() has returned; bench_done = $600d; idle forever.
;
; The latches are read in eight-bit mode through long addresses (DBR = $7E
; does not mirror the I/O ports). Elapsed master clocks are
; (v1 - v0) * 1364 + (h1 - h0) * 4, less the calibrated fixed overhead of
; an empty bench_run (the jsl/rtl and the tail of the first latch).

.SECTION "harness.code" SUPERFREE

main:
  rep #$30
  stz.w bench_phase
  stz.w bench_done
  stz.w bench_out_count
  stz.w bench_wrap
  ldx.w #62
harness_clear_out:
  stz.w bench_out,x
  dex
  dex
  bpl harness_clear_out
  jsl bench_setup
  rep #$30
  lda.w #1
  sta.w bench_phase

  sei                         ; the V-IRQ below only raises TIMEUP, never an interrupt
  sep #$20
  lda.b #0
  sta.l $004200               ; NMI, IRQ and auto-joypad off
  lda.b #$80
  sta.l $004201               ; WRIO bit 7: $2137 reads latch the counters
  lda.b #224
  sta.l $004209               ; VTIME = 224: TIMEUP ($4211 bit 7) is set once a
  lda.b #0                    ; frame, a line before the VBlank bench_run starts
  sta.l $00420A               ; in, and stays set until read
  lda.b #$20
  sta.l $004200               ; V-IRQ on (masked by I), NMI still off
  rep #$20

  jsr harness_wait_vblank
  jsr harness_wait_vblank

  lda.w #2
  sta.w bench_phase

  sep #$20
  lda.l $004211               ; clear TIMEUP (line 224 has just passed)
  lda.l $00213F               ; reset the $213C/$213D byte flip-flops
  lda.l $002137               ; latch H/V: start
  lda.l $00213C
  sta.w bench_h0
  lda.l $00213C
  and.b #1
  sta.w bench_h0 + 1
  lda.l $00213D
  sta.w bench_v0
  lda.l $00213D
  and.b #1
  sta.w bench_v0 + 1
  lda.l $00213F
  rep #$30
  jsl bench_run
  sep #$20
  lda.l $002137               ; latch H/V: end
  rep #$30

harness_after_run:
  sep #$20
  lda.l $00213C
  sta.w bench_h1
  lda.l $00213C
  and.b #1
  sta.w bench_h1 + 1
  lda.l $00213D
  sta.w bench_v1
  lda.l $00213D
  and.b #1
  sta.w bench_v1 + 1
  lda.l $00213F
  lda.l $004211               ; TIMEUP: line 224 came round again during
  and.b #$80                  ; bench_run, so it took (nearly) a frame or
  sta.w bench_wrap            ; longer and the H/V difference may have wrapped
  lda.b #0
  sta.l $004200               ; V-IRQ off again
  rep #$30
  lda.w #3
  sta.w bench_phase

  jsr harness_wait_vblank
  jsr harness_wait_vblank

  rep #$30
  jsl bench_check
  rep #$30
  lda.w #4
  sta.w bench_phase
  lda.w #$600d
  sta.w bench_done

harness_idle:
  bra harness_idle

; Waits for the start of the next VBlank (leaves A 16-bit).
harness_wait_vblank:
  sep #$20
harness_wait_leave:
  lda.l $004212
  bmi harness_wait_leave      ; still inside a VBlank: wait for it to end
harness_wait_enter:
  lda.l $004212
  bpl harness_wait_enter
  rep #$20
  rts

.ENDS

.BASE $00
.RAMSECTION "harness.ram" BANK $7E SLOT 2
bench_phase     dsb 2
bench_done      dsb 2
bench_h0        dsb 2
bench_v0        dsb 2
bench_h1        dsb 2
bench_v1        dsb 2
bench_wrap      dsb 2
bench_out_count dsb 2
bench_out       dsb 64
.ENDS
