/*
 * heapcheck.h
 *
 * Host-side (native driver) integrity checker + utilization meter for
 * the cc65 heap running inside the emulated Atari, used by the resident
 * test image. See heapcheck.c for the full rationale; in brief:
 *
 * cc65's malloc/free (common/_heap.s) is a bump pointer plus a
 * doubly-linked, address-sorted free list of
 *   struct freeblock { u16 size; u16 next; u16 prev; }
 * There is NO list of *live* allocations kept, so we cannot enumerate
 * leaked blocks individually -- but we CAN, purely by reading emulated
 * RAM from the host (zero 6502-side footprint, so it never perturbs the
 * very heap pressure we're measuring):
 *   1. validate the free list's structural invariants (catches heap
 *      corruption, e.g. a descending software stack that overran a live
 *      block's neighbour, or a double free);
 *   2. compute exact free bytes (free-list total + virgin bump gap),
 *      which the runner samples at every test boundary to report peak
 *      utilization and to detect leak DRIFT across overlay boundaries
 *      (each overlay swap calls free_other_buffers(), so free bytes
 *      should return to a stable baseline; a monotonic decline = a
 *      leak in the modified src/ application code under test).
 *
 * The four heap bookkeeping variables are cc65 runtime-library symbols
 * built without -g, so they are absent from the .dbg -- but present in
 * the linker .map. heap_resolve_syms() reads their addresses from there.
 */
#ifndef HEAPCHECK_H
#define HEAPCHECK_H

/*
 * Addresses (within emulated RAM) of cc65's heap bookkeeping VARIABLES.
 * These are the storage locations of the pointers themselves, not the
 * heap payload; read each one's 16-bit little-endian value out of
 * emulated RAM to get the live heap state. Resolved once from the .map.
 */
typedef struct {
    unsigned heapptr;    /* &__heapptr   (bump pointer, grows up)      */
    unsigned heapend;    /* &__heapend   (heap ceiling, constant)      */
    unsigned heapfirst;  /* &__heapfirst (free-list head, lowest addr) */
    unsigned heaplast;   /* &__heaplast  (free-list tail, highest)     */
    int      ok;         /* nonzero iff all four were resolved         */
} heap_syms_t;

/* Result of a single heap probe. */
typedef struct {
    unsigned heapptr;      /* live __heapptr value                     */
    unsigned heapend;      /* live __heapend value                     */
    unsigned free_bytes;   /* free-list total + (heapend - heapptr)    */
    unsigned free_blocks;  /* number of blocks on the free list        */
    unsigned largest_free; /* largest single free block (fragmentation)*/
    int      corrupt;      /* 0 = clean; nonzero = an invariant failed */
    const char *err;       /* human description when corrupt           */
} heap_probe_t;

/*
 * Resolve the four cc65 heap variable addresses from a linker .map file.
 * Returns 0 and fills *out (with out->ok = 1) on success; returns -1 if
 * the map could not be opened or any symbol was missing (out->ok = 0).
 */
int heap_resolve_syms(const char *map_path, heap_syms_t *out);

/*
 * Look up one symbol's address in a linker .map file (e.g. "_bheadp" for
 * the C global bheadp). Returns 0 and sets *out_addr on success, -1 if
 * the map could not be opened or the symbol is not listed.
 */
int map_resolve_symbol(const char *map_path, const char *name, unsigned *out_addr);

/*
 * Read the live heap state out of emulated RAM (`mem`, a 64 KiB array)
 * and validate the free list, filling *out. Never mutates emulated RAM.
 * out->corrupt is set (with out->err) on the first invariant violation;
 * out->free_bytes is still reported best-effort in that case.
 */
void heap_probe(const unsigned char *mem, const heap_syms_t *s,
                heap_probe_t *out);

#endif /* HEAPCHECK_H */
