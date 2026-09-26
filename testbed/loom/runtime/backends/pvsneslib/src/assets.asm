.include "hdr.asm"

; The logical 32-byte asset crosses a physical LoROM bank boundary after byte
; 16. It is intentionally represented by two labels because $06:ffff does not
; linearly advance to the next LoROM window at $07:8000.
.BANK 6 SLOT $8000
.ORG $7ff0
.SECTION "loom.asset.bank_crossing.first" FORCE KEEP
loom_pvs_asset_bank_crossing_first:
  .db $0b,$30,$55,$7a,$9f,$c4,$e9,$0e
  .db $33,$58,$7d,$a2,$c7,$ec,$11,$36
.ENDS

.BANK 7 SLOT $8000
.ORG $0000
.SECTION "loom.asset.bank_crossing.second" FORCE KEEP
loom_pvs_asset_bank_crossing_second:
  .db $5b,$80,$a5,$ca,$ef,$14,$39,$5e
  .db $83,$a8,$cd,$f2,$17,$3c,$61,$86
.ENDS

.SECTION "loom.asset.oam_palette" SUPERFREE KEEP
loom_pvs_asset_oam_palette:
  .db $00,$00,$1f,$00,$e0,$03,$00,$00
  .db $00,$00,$00,$00,$00,$00,$00,$00
  .db $00,$00,$00,$00,$00,$00,$00,$00
  .db $00,$00,$00,$00,$00,$00,$00,$00
.ENDS

.SECTION "loom.asset.oam_tiles" SUPERFREE KEEP
loom_pvs_asset_oam_tiles:
  .db $ff,$00,$ff,$00,$ff,$00,$ff,$00
  .db $ff,$00,$ff,$00,$ff,$00,$ff,$00
  .db $00,$00,$00,$00,$00,$00,$00,$00
  .db $00,$00,$00,$00,$00,$00,$00,$00
  .db $00,$ff,$00,$ff,$00,$ff,$00,$ff
  .db $00,$ff,$00,$ff,$00,$ff,$00,$ff
  .db $00,$00,$00,$00,$00,$00,$00,$00
  .db $00,$00,$00,$00,$00,$00,$00,$00
.ENDS

; One original looping BRR block. Header $83 selects range 8, filter 0, end,
; and loop; the alternating samples provide a deterministic non-silent witness.
.SECTION "loom.asset.audio_tone" SUPERFREE KEEP
loom_pvs_audio_tone:
  .db $83,$17,$3a,$5c,$7e,$e7,$c5,$a3,$81
.ENDS

.RAMSECTION "loom.wram.high" BANK $7f SLOT 3 ORGA $ff00 FORCE
loom_pvs_wram_high dsb 64
.ENDS
