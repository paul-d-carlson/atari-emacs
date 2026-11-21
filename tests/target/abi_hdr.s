;
; abi_hdr.s
;
; Tiny hand-written DOS binary-load chunk header for the ABITABLE
; segment (see abi_table.h / abi_export.c / test-resident.cfg).
;
; ld65's Atari target auto-generates this 4-byte {start,end} address
; header for the well-known MAINHDR/SYSCHKHDR/etc. segment names (their
; contents come from cc65's own crt0/runtime object files) -- but it has
; no idea our custom "ABITABHDR" segment should hold one, since nothing
; else contributes to it. This stub supplies it explicitly, computed
; entirely from linker-exported addresses (__ABITABLE_RUN__/_SIZE__, see
; test-resident.cfg's `define = yes` on the ABITABLE segment/ABITAB
; memory area) so it always matches wherever the ABITABLE segment
; actually lands, with zero hand-maintained addresses to keep in sync.
;
        .export __ABITABHDR_START__

        .import __ABITABLE_RUN__
        .import __ABITABLE_SIZE__

        .segment "ABITABHDR"

__ABITABHDR_START__:
        .word   __ABITABLE_RUN__
        .word   __ABITABLE_RUN__ + __ABITABLE_SIZE__ - 1
