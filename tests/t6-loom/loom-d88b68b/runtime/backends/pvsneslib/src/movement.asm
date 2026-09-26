.include "hdr.asm"

.accu 16
.index 16
.16bit

; The console-side tile-collision probe, in assembly because 816-tcc spends
; roughly 440 instructions on it in C -- most of them spilling a dozen locals
; to a 28-byte stack frame and reading them back. The same work here is about
; sixty. It is called two to ten times a tick: once per axis for the player
; and once per axis for every moving actor.
;
; The portable C rendition stays in runtime/src/movement.c and is what the
; host suites under runtime/tests exercise. This is the measured hot loop the
; language decision reserves assembly for; the two must agree exactly, and the
; ROM tests pin the player's position after walking into walls, which is what
; actually holds them together.
;
; The scene's fields arrive by value rather than as a struct pointer, so this
; file needs no knowledge of LoomMovementScene's layout and cannot drift from
; it the way the OAM builder once drifted from LoomMode1Sprite.
;
; 816-tcc ABI: JSL/RTL, the first argument at 4,s (5,s after our php), data
; pointers as four bytes (offset word, bank word), 8-bit results in tcc__r0,
; and tcc__mul taking its operands in tcc__r9/tcc__r10 and returning in A.
;
; loom_u8 loom_pvs_movement_box_blocked(const loom_u8 *cells,   ;  5,s /  7,s
;                                       loom_u16 collision_width, ;  9,s
;                                       loom_u16 pixel_width,   ; 11,s
;                                       loom_u16 pixel_height,  ; 13,s
;                                       loom_s16 left,          ; 15,s
;                                       loom_s16 top,           ; 17,s
;                                       loom_s16 right,         ; 19,s
;                                       loom_s16 bottom)        ; 21,s
;
; Scratch: r1 first_x, r2 last_x, r3 the row being scanned, r4 last_y,
; r5 the row's base pointer, r0 the result.

.SECTION "loom.pvs.movement.code" SUPERFREE KEEP

loom_pvs_movement_box_blocked:
  php
  rep #$30

  ; A box that starts off the map is solid, exactly as the C reads it.
  lda 15,s
  bmi loom_pvs_movement_solid
  lda 17,s
  bmi loom_pvs_movement_solid
  ; right >= pixel_width. A negative right makes the C comparison false, so
  ; skip it rather than letting an unsigned compare call it solid.
  lda 19,s
  bmi loom_pvs_movement_skip_right
  cmp 11,s
  bcs loom_pvs_movement_solid
loom_pvs_movement_skip_right:
  lda 21,s
  bmi loom_pvs_movement_skip_bottom
  cmp 13,s
  bcs loom_pvs_movement_solid
loom_pvs_movement_skip_bottom:
  bra loom_pvs_movement_setup

  ; The two exits sit here so the bounds checks above reach them with ordinary
  ; branches; the scan below is more than 127 bytes away and uses brl.
loom_pvs_movement_solid:
  rep #$30
  lda #1
  sta.b tcc__r0
  plp
  rtl

loom_pvs_movement_clear:
  rep #$30
  stz.b tcc__r0
  plp
  rtl

loom_pvs_movement_setup:
  ; The row stride is one multiply, hoisted out of the loop the C left it in.
  ; Nothing of ours is live across the call, so tcc__mul may clobber freely.
  lda 9,s
  sta.b tcc__r9
  lda 17,s
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r10
  jsr.l tcc__mul
  clc
  adc 5,s
  sta.b tcc__r5
  lda 7,s
  adc #0
  sta.b tcc__r5h

  ; The tile span the box covers. These are unsigned shifts, which is what
  ; the C's division by a power of two compiles to.
  lda 15,s
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r1
  lda 19,s
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r2
  lda 17,s
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r3
  lda 21,s
  lsr a
  lsr a
  lsr a
  lsr a
  sta.b tcc__r4

loom_pvs_movement_row:
  rep #$20
  lda.b tcc__r3
  cmp.b tcc__r4
  beq loom_pvs_movement_row_scan
  bcc loom_pvs_movement_row_scan
  brl loom_pvs_movement_clear
loom_pvs_movement_row_scan:
  ldy.b tcc__r1
  sep #$20
loom_pvs_movement_cell:
  cpy.b tcc__r2
  beq loom_pvs_movement_read
  bcs loom_pvs_movement_row_next
loom_pvs_movement_read:
  lda.b [tcc__r5],y
  beq loom_pvs_movement_next_cell
  brl loom_pvs_movement_solid
loom_pvs_movement_next_cell:
  iny
  bra loom_pvs_movement_cell
loom_pvs_movement_row_next:
  rep #$20
  lda.b tcc__r5
  clc
  adc 9,s
  sta.b tcc__r5
  lda.b tcc__r5h
  adc #0
  sta.b tcc__r5h
  inc.b tcc__r3
  bra loom_pvs_movement_row

.ENDS
