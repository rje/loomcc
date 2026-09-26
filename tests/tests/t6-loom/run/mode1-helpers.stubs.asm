.include "hdr.asm"
; Link stubs for mode1-helpers.c (scripts/t6-stubs.py): symbols the
; included runtime unit references but the driver never reaches.
.BASE $00
.RAMSECTION "t6_stubs_mode1_helpers" BANK $7E SLOT 2
.ENDS
