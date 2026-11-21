# Changelog

All notable changes to this project are documented here. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
the project uses [Semantic Versioning](https://semver.org/).

## [0.2.0] - 2026-11-21

### Changed

- CI uses the Node 24 releases of the GitHub Actions it depends on.
- The release job fails if the pushed tag does not match `VERSION` in
  the Makefile, so a release can never ship a misnamed archive.

## [0.1.0] - 2026-11-21

First public release.

### Added

- MicroEMACS 3.6 ported to the Atari 8-bit with cc65: a 40x24 `conio`
  terminal driver, `D:` file I/O with ATASCII line endings, a directory
  listing, multiple buffers with automatic eviction of unchanged buffers,
  and an on-screen key reference (`M-?`).
- A single make-based build: `make` for `me.xex`, `make atr` for a
  bootable DOS 2.5 disk, `make deps` to fetch the pinned, checksummed
  third-party tools.
- A build-time guard that fails if the load address is not above the
  DOS 2.5 MEMLO, and a checksum check on the bundled DOS.
- 181 unit tests running the real 6502 code under libatari800, with heap
  integrity checks and line coverage; end-to-end integration scenarios
  against the shipped binary; and a heap/stack/disk soak test.
- GitHub Actions CI on Ubuntu 24.04: builds, runs the unit and
  integration tests, reports code size and heap use, and publishes a
  release archive for `v*` tags.

### Fixed

- Lines longer than 255 bytes are now read and written correctly:
  `ffputline()` and `ffgetline()` used 8-bit loop counters that wrapped
  around.

[0.2.0]: https://github.com/paul-d-carlson/atari-emacs/releases/tag/v0.2.0
[0.1.0]: https://github.com/paul-d-carlson/atari-emacs/releases/tag/v0.1.0
