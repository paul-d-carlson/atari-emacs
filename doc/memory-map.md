# Memory map

The editor runs on a 48 KB Atari 8-bit with BASIC disabled, booted from
Atari DOS 2.5. Every byte matters: whatever the program image does not
use becomes heap, and the heap holds all buffer text.

```
$0000-$00FF  zero page   OS: $00-$81 | cc65 runtime and register
                         variables: $82-$A0
$0100-$01FF              6502 hardware stack
$0700-$1CFB              DOS 2.5 (DOS.SYS, buffers); MEMLO = $1CFC
$1E00-$9B12  me.xex      STARTUP, ONCE, CODE, RODATA, DATA, INIT, BSS
$9B13-$BA1F  heap        ~7.8 KB for buffers, lines and the kill buffer
$BA20-$BC1F  C stack     512 bytes (cc65 software stack)
$BC20-$BFFF              GRAPHICS 0 display list and screen memory
$C000-$FFFF              OS ROM (and hardware registers at $D000-$D7FF)
```

Figures are from `build/me.map` for the current release. The heap
boundary moves whenever code or static data changes size, so read the
map (or the `make test-unit` heap report) after any change.

## Where the numbers come from

- **Load address, `$1E00`.** Set by `STARTADDRESS` in `src/atari.cfg`.
  DOS 2.5 with its default configuration sets MEMLO to `$1CFC`, which was
  measured by booting the bundled `DOS.SYS` under libatari800 and reading
  `$02E7`. The build fails if `STARTADDRESS` is not above `DOS25_MEMLO`
  (in `mk/config.mk`), and `third_party/atari-dos25/SHA256SUMS` makes sure
  the DOS on the disk is the one that was measured.
- **Top of memory, `$BC20`.** The bottom of the GRAPHICS 0 screen on a
  48 KB machine. The program segment (`MAIN` in `src/atari.cfg`) ends
  there, minus the C stack.
- **Stack, 512 bytes.** `__STACKSIZE__` in `src/atari.cfg`. It is large
  enough for the deepest call chain; `make stress` reports the maximum
  depth seen and the remaining margin.
- **Zero page.** `--register-space 24` (in `APP_CFLAGS`) lets cc65 keep
  up to 24 bytes of `register` variables in zero page, which shrinks the
  code. Larger values gave no further savings.

## The system-check chunk

cc65's Atari runtime adds a small loader chunk (`SYSCHK`, at `$2E00`)
that checks the machine has enough memory before the main program loads.
It runs once, and the main program then overwrites it, so it costs no
memory at run time.

## The unit-test image

The resident unit-test image uses its own linker config
(`tests/target/test-resident.cfg`) with the same load address and a
256-byte stack. It reserves a fixed overlay window just below the stack
for the test suites, so its heap is smaller (about 3.2 KB). The heap
report printed by `make test-unit` is for that image, not for `me.xex`.
