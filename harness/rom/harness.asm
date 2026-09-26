.include "hdr.asm"

.accu 16
.index 16
.16bit

; loomcc-tests ROM harness: `main` for one execute test.
;
; crt0 calls main with D = $0000, DBR = $7E, native mode, A/X/Y 16-bit.
; The test's own main is renamed loomcc_test_main (-Dmain=loomcc_test_main)
; and abort/exit are renamed loomcc_test_abort/loomcc_test_exit
; (-Dabort=... -Dexit=...), so PVSnesLib's libc versions are never reached.
;
; Outcome, read by the runner through loom-emulator watches:
;   test_done   = 1 once the test has finished (any way)
;   test_status = $600d  main returned; test_result holds its return value
;                 $dead  abort() was called
;                 $e817  exit() was called; test_result holds its argument
;                 $c4ec  a CHECK failed; test_result holds its line
;                        (loomcc-test.h's CHECK calls loomcc_test_fail_line)
;   test_result = the 16-bit return value (0 means pass)

.SECTION "loomcc_harness.code" SUPERFREE

main:
  rep #$30
  lda.w #0
  sta.l test_done
  sta.l test_status
  sta.l test_result
  jsl loomcc_test_main
  rep #$30
  lda.b tcc__r0
  sta.l test_result
  lda.w #$600d
  sta.l test_status
  lda.w #1
  sta.l test_done
harness_idle:
  bra harness_idle

; void loomcc_test_abort(void)
loomcc_test_abort:
  rep #$30
  lda.w #$dead
  sta.l test_status
  lda.w #1
  sta.l test_done
harness_abort_idle:
  bra harness_abort_idle

; void loomcc_test_fail_line(int line): line at 4,s (816-tcc ABI)
loomcc_test_fail_line:
  rep #$30
  lda 4,s
  sta.l test_result
  lda.w #$c4ec
  sta.l test_status
  lda.w #1
  sta.l test_done
harness_fail_idle:
  bra harness_fail_idle

; void loomcc_test_exit(int status): status at 4,s (816-tcc ABI)
loomcc_test_exit:
  rep #$30
  lda 4,s
  sta.l test_result
  lda.w #$e817
  sta.l test_status
  lda.w #1
  sta.l test_done
harness_exit_idle:
  bra harness_exit_idle

.ENDS

.BASE $00
.RAMSECTION "loomcc_harness.ram" BANK $7E SLOT 2
test_done       dsb 2
test_status     dsb 2
test_result     dsb 2
.ENDS
