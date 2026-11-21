/*
 * overlay_loader.c
 *
 * See overlay_loader.h. Uses cc65's POSIX-style low-level file I/O
 * (open/read/close from fcntl.h/unistd.h -- the same CIO-backed D:
 * layer that stdio's fopen/fread are built on top of) for a raw binary
 * read, rather than MicroEMACS's own line-oriented ffropen()/
 * ffgetline() pair -- an overlay image is an arbitrary binary blob
 * (compiled 6502 code + data), not line-structured text, and may
 * contain byte values (embedded newlines, 0x9B record-terminator bytes,
 * etc.) that ffgetline()'s line-splitting would corrupt.
 *
 * open()/read()/close() are used instead of fopen()/fread()/fclose()
 * deliberately: they operate on raw bytes with no text-mode/line-ending
 * translation and no buffered-stdio FILE* struct/heap allocation in
 * the way, which stdio's fopen() was found to interact badly with when
 * combined with a large (8KB) single read into a fixed address.
 */
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "abi_table.h"
#include "overlay_loader.h"

unsigned char overlay_load(const char *fname)
{
    int fd;
    int n;
    unsigned int total;
    unsigned char *dst;

    fd = open(fname, O_RDONLY);
    if (fd == -1)
        return 0;

    memset((void *)OVERLAY_BASE, 0, OVERLAY_SIZE);

    dst = (unsigned char *)OVERLAY_BASE;
    total = 0;
    while (total < OVERLAY_SIZE) {
        unsigned int remain = OVERLAY_SIZE - total;
        n = read(fd, dst + total, remain);
        if (n <= 0)
            break;
        total += (unsigned int)n;
    }

    close(fd);

    return (total > 0) ? 1 : 0;
}
