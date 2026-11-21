/*
 * overlay_loader.h
 *
 * Resident-side loader for the disk-overlay test architecture: reads a
 * suite's separately-compiled overlay image (test-overlay.cfg, a raw
 * headerless binary blob) from a real D: disk file directly into the
 * fixed OVERLAY_BASE/OVERLAY_SIZE window (see abi_table.h), so it can
 * be invoked in place. See test_main_resident.c for how each suite is
 * dispatched afterward.
 */
#ifndef OVERLAY_LOADER_H
#define OVERLAY_LOADER_H

/* Zeroes the whole overlay window (so a fresh suite's BSS starts clean,
 * uncontaminated by whatever the previous suite left behind), then
 * reads `fname` (a real D: file, e.g. "D:OVBUF.BIN") into it. Returns
 * TRUE (1) on success, FALSE (0) if the file could not be opened. */
unsigned char overlay_load(const char *fname);

#endif /* OVERLAY_LOADER_H */
