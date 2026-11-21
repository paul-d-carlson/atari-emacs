/*
 * atr_dos25.h
 *
 * Host-side (no emulation) reader for a single file's contents out of an
 * Atari DOS 2.x-formatted .atr disk image. Used by integration scenarios
 * to verify what the real product actually wrote to "disk" after a
 * keystroke script runs (e.g. after C-X C-W / filewrite), without
 * needing to reboot the emulator or add any instrumentation to the
 * 6502 binary.
 *
 * Format reference (confirmed against "De Re Atari" ch.9 and
 * atarimagazines.com's DOS comparison article):
 *   - 16-byte ATR file header (wMagic=$0296, wSecSize, ...); sector data
 *     starts at file offset 16. Sector N (1-indexed) is at offset
 *     16 + (N-1)*wSecSize.
 *   - This project's disks always use 128-byte sectors (both the
 *     720-sector "Dos25 720" and 1040-sector "Dos25 1040" -b dir2atr
 *     formats), so wSecSize is required to be 128.
 *   - VTOC at sector 360 (not needed for reading a known file by name).
 *   - Directory at sectors 361-368, 8 entries/sector, 16 bytes/entry:
 *       byte 0:      flag (bit7=deleted, bit6=in-use, bit5=locked,
 *                    bit0=open-for-output; byte==0x00 = never-used slot)
 *       bytes 1-2:   sector count (LE)
 *       bytes 3-4:   starting sector number (LE)
 *       bytes 5-12:  8-char filename, space-padded
 *       bytes 13-15: 3-char extension, space-padded
 *   - File sector chain: last 3 bytes of every 128-byte sector are link
 *     overhead (125 usable data bytes/sector otherwise):
 *       byte 125: bits 7-2 = file number (directory slot, integrity
 *                 check only), bits 1-0 = high 2 bits of the forward
 *                 sector pointer
 *       byte 126: low 8 bits of the forward sector pointer; a combined
 *                 pointer of 0 means this is the last sector
 *       byte 127: bit 7 = "short sector" flag; bits 6-0 = number of
 *                 valid data bytes in this sector (only meaningful/used
 *                 here for the last sector in the chain)
 */
#ifndef ATR_DOS25_H
#define ATR_DOS25_H

#include <stddef.h>

/*
 * Reads the named file (DOS 8.3 style, e.g. "POEM.TXT"; an optional
 * "D:" device prefix is accepted and ignored) out of the DOS 2.x
 * directory/sector-chain found in the .atr at atr_path.
 *
 * On success returns 0, sets out_data to a malloc'd buffer (caller must
 * free it) holding the file's raw bytes, and out_len to its length.
 * On failure (bad/unsupported ATR header, file not found, malformed
 * directory/chain) returns -1 and leaves out_data/out_len untouched.
 */
int atr_dos25_read_file(const char *atr_path, const char *dos_name,
                         unsigned char **out_data, size_t *out_len);

/*
 * Reads the free-sector count out of the VTOC (sector 360, bytes 3-4,
 * little-endian -- standard DOS 2.x VTOC layout, same sector this file's
 * atr_dos25.c already names via VTOC_SECTOR). Returns 0 and fills
 * *out_free on success, -1 on a bad/unreadable ATR.
 */
int atr_dos25_free_sectors(const char *atr_path, unsigned *out_free);

#endif /* ATR_DOS25_H */
