/*
 * heapcheck.c
 *
 * Host-side cc65 heap integrity checker + free-byte meter. See
 * heapcheck.h for the design rationale. This code runs in the native
 * driver process and only ever READS emulated Atari RAM, so it adds
 * exactly zero bytes of code/data/heap to the 6502 image under test --
 * critical, because the whole point is to measure a heap whose pressure
 * is razor-thin, without disturbing it.
 *
 * cc65 heap layout (common/_heap.s, asminc/_heap.inc), confirmed against
 * this build's linker map:
 *   __heaporg  : constant bottom of the heap (NOTE: often stripped by
 *                ld65 when unreferenced -- as in this build -- so we take
 *                the earliest observed __heapptr as the origin instead).
 *   __heapptr  : bump pointer. [__heaporg, __heapptr) has been handed out
 *                at least once; [__heapptr, __heapend) is still virgin.
 *   __heapend  : constant ceiling; heap can never grow past here.
 *   __heapfirst/__heaplast : head/tail of an ADDRESS-SORTED, coalesced,
 *                doubly-linked free list of
 *                  struct freeblock { u16 size; u16 next; u16 prev; }.
 *   Allocated blocks carry a 2-byte size header (usedblock) but are NOT
 *   threaded onto any list, so live blocks cannot be enumerated -- only
 *   the free list + the bump gap give us exact free bytes.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "heapcheck.h"

/* cc65 asminc/_heap.inc: .struct freeblock { size .word; next .addr; prev .addr } */
#define FB_SIZE 0
#define FB_NEXT 2
#define FB_PREV 4
#define HEAP_MIN_BLOCKSIZE 6   /* == sizeof(freeblock) */
#define MAX_FREE_BLOCKS 8192   /* cycle guard: heap can't hold more */

static unsigned rd16(const unsigned char *mem, unsigned addr)
{
    return (unsigned)mem[addr & 0xFFFF] | ((unsigned)mem[(addr + 1) & 0xFFFF] << 8);
}

int heap_resolve_syms(const char *map_path, heap_syms_t *out)
{
    /* The four names we need, paired with where to store each address.
     * ld65 maps can list two symbols per physical line ("NAME  ADDR  FLG
     * NAME  ADDR  FLG"), so we tokenize the whole file and, whenever a
     * token equals one of these names, treat the NEXT token as its hex
     * address -- robust to the two-column layout. */
    struct { const char *name; unsigned *slot; } want[] = {
        { "__heapptr",   &out->heapptr   },
        { "__heapend",   &out->heapend   },
        { "__heapfirst", &out->heapfirst },
        { "__heaplast",  &out->heaplast  },
    };
    const int NWANT = (int)(sizeof(want) / sizeof(want[0]));
    int found[4] = { 0, 0, 0, 0 };
    int nfound = 0, i;
    FILE *f;
    char line[512];

    memset(out, 0, sizeof(*out));

    f = fopen(map_path, "r");
    if (f == NULL)
        return -1;

    while (nfound < NWANT && fgets(line, sizeof(line), f) != NULL) {
        char *tok = strtok(line, " \t\r\n");
        while (tok != NULL) {
            for (i = 0; i < NWANT; i++) {
                if (!found[i] && strcmp(tok, want[i].name) == 0) {
                    char *addr = strtok(NULL, " \t\r\n");
                    if (addr != NULL) {
                        *want[i].slot = (unsigned)(strtoul(addr, NULL, 16) & 0xFFFF);
                        found[i] = 1;
                        nfound++;
                    }
                    tok = NULL; /* force re-fetch below */
                    break;
                }
            }
            tok = strtok(NULL, " \t\r\n");
        }
    }
    fclose(f);

    out->ok = (nfound == NWANT);
    return out->ok ? 0 : -1;
}

int map_resolve_symbol(const char *map_path, const char *name, unsigned *out_addr)
{
    FILE *f;
    char line[512];
    int found = 0;

    f = fopen(map_path, "r");
    if (f == NULL)
        return -1;

    /* Same two-column-tolerant tokenizing as heap_resolve_syms(). */
    while (!found && fgets(line, sizeof(line), f) != NULL) {
        char *tok = strtok(line, " \t\r\n");
        while (tok != NULL) {
            if (strcmp(tok, name) == 0) {
                char *addr = strtok(NULL, " \t\r\n");
                if (addr != NULL) {
                    *out_addr = (unsigned)(strtoul(addr, NULL, 16) & 0xFFFF);
                    found = 1;
                }
                break;
            }
            tok = strtok(NULL, " \t\r\n");
        }
    }
    fclose(f);
    return found ? 0 : -1;
}

void heap_probe(const unsigned char *mem, const heap_syms_t *s, heap_probe_t *out)
{
    unsigned blk, prev, size, next;
    unsigned n;

    memset(out, 0, sizeof(*out));
    if (!s->ok) {
        out->corrupt = 1;
        out->err = "heap symbols unresolved";
        return;
    }

    out->heapptr = rd16(mem, s->heapptr);
    out->heapend = rd16(mem, s->heapend);

    /* Bump-pointer sanity: heapptr must never exceed the ceiling. */
    if (out->heapptr > out->heapend) {
        out->corrupt = 1;
        out->err = "heapptr > heapend (bump pointer corrupt)";
        return;
    }
    out->free_bytes = out->heapend - out->heapptr;   /* virgin bump gap */

    /* Walk the address-sorted, doubly-linked free list. */
    blk = rd16(mem, s->heapfirst);
    prev = 0;
    for (n = 0; blk != 0; n++) {
        if (n >= MAX_FREE_BLOCKS) {
            out->corrupt = 1;
            out->err = "free list cycle (exceeded max block count)";
            return;
        }
        /* Every free block must lie strictly inside the allocated region
         * [.., heapptr): the bump pointer is above anything ever handed
         * out, and a free block below the ZP/heap floor is nonsense. */
        if (blk < 0x0100 || blk >= out->heapptr) {
            out->corrupt = 1;
            out->err = "free block address out of heap bounds";
            return;
        }
        size = rd16(mem, blk + FB_SIZE);
        next = rd16(mem, blk + FB_NEXT);
        if (size < HEAP_MIN_BLOCKSIZE || blk + size > out->heapptr) {
            out->corrupt = 1;
            out->err = "free block size invalid / overruns heapptr";
            return;
        }
        /* prev back-link must point at the block we just came from. */
        if (rd16(mem, blk + FB_PREV) != prev) {
            out->corrupt = 1;
            out->err = "free block prev back-link mismatch";
            return;
        }
        /* List is address-sorted and coalesced: the next block must sit
         * strictly ABOVE this one's end (touching blocks would have been
         * merged; a lower/overlapping next means a smashed link). */
        if (next != 0 && next < blk + size) {
            out->corrupt = 1;
            out->err = "free list not ascending / blocks overlap";
            return;
        }
        out->free_bytes += size;
        out->free_blocks++;
        if (size > out->largest_free)
            out->largest_free = size;
        prev = blk;
        blk = next;
    }

    /* The last block we visited must be the recorded tail (0 if empty). */
    if (prev != rd16(mem, s->heaplast)) {
        out->corrupt = 1;
        out->err = "free list tail (__heaplast) mismatch";
        return;
    }
}
