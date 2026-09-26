.include "hdr.asm"
; Link stubs for mode1-stream.c (scripts/t6-stubs.py): symbols the
; included runtime unit references but the driver never reaches.
.BASE $00
.RAMSECTION "t6_stubs_mode1_stream" BANK $7E SLOT 2
.ENDS
