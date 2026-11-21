# atari-emacs

MicroEMACS 3.6 for the Atari 8-bit: an Emacs-style editor with multiple
buffers, disk file editing and an on-screen key reference, in under 48 KB.

- Runs on any Atari 400/800/XL/XE with 48 KB or more, BASIC disabled.
- Boots as `AUTORUN.SYS` from a standard Atari DOS 2.5 disk.
- About 30 KB of code, leaving about 7.8 KB of heap for text
  (see [doc/memory-map.md](doc/memory-map.md)).
- Covered by 181 unit tests and end-to-end editing scenarios that run the
  real 6502 binary under emulation (see [tests/README.md](tests/README.md)).

## Building

### Requirements

| Tool | Version | Used for |
|------|---------|----------|
| [cc65](https://cc65.github.io/) | 2.19 | compiling the editor |
| GNU make | 3.81 or later | the build |
| C and C++ compilers | recent gcc or clang | `dir2atr` and the test drivers |
| curl, tar | | `make deps` |
| [atari800](https://atari800.github.io/) | optional | `make run` |

macOS (Homebrew):

```
brew install cc65 atari800
```

Debian or Ubuntu:

```
sudo apt install cc65 build-essential curl atari800
```

### Build and run

```
git clone https://github.com/paul-d-carlson/atari-emacs.git
cd atari-emacs
make deps     # once: fetch and build dir2atr and libatari800
make          # build/me.xex
make atr      # build/me.atr, a bootable DOS 2.5 disk
make run      # boot build/me.atr in atari800
```

`make` on its own needs only cc65. `make deps` downloads pinned releases
of AtariSIO (for `dir2atr`, which builds disk images) and atari800 (whose
`libatari800` runs the tests), checks their SHA-256 checksums, and builds
them under `build/deps/`. Nothing is installed system-wide.

`make help` lists every target and the variables you can override, such
as `CL65=`, `HOST_CC=`, `DIR2ATR=` and `DOS25_DIR=`.

### Testing

```
make test              # unit + integration suites
make test-unit         # 181 unit tests, heap report, line coverage
make test-integration  # end-to-end editing sessions against build/me.xex
make stress            # long-running soak test; stop with Q + RETURN
```

Run `make test` before sending changes, and compare the heap figures it
reports with those from before your change.

## Running

**Emulator.** Boot `build/me.atr` as drive 1 with BASIC disabled. In
[Altirra](https://www.virtualdub.org/altirra.html), use *File > Boot
Image*; with atari800, use `make run`.

**Real hardware.** Write `build/me.atr` to a floppy, or serve it from an
SIO drive emulator such as SIO2PC, SIO2SD or FujiNet. Hold **OPTION**
while powering on an XL/XE to disable BASIC.

**Your own DOS disk.** `build/me.xex` is a standard Atari binary file.
Copy it to a DOS 2.5 disk (as `AUTORUN.SYS` to start it at boot), along
with `EMACS.HLP` for the help screen. It loads at `$1E00`, so it needs a
DOS whose MEMLO is below that; DOS 2.5 uses `$1CFC`.

File names without a device are opened on `D:` and uppercased, so
`notes.txt` and `D1:NOTES.TXT` are the same file. Lines end with the
Atari EOL character (`$9B`), so files are compatible with other Atari
programs.

## Using the editor

`ESC` is the Meta prefix: `M-<` means press `ESC`, then `<`. `^X` means
Ctrl-X. Press `M-?` at any time to show a summary on screen.

| Keys | Command |
|------|---------|
| `^F` `^B` | forward / backward one character |
| `^N` `^P` | next / previous line |
| `^A` `^E` | start / end of line |
| `^V`, `^Z` or `M-V` | page down, page up |
| `M-<` `M->` | start / end of buffer |
| `M-G` | go to line number |
| `DELETE` or `^H` | delete the previous character |
| `^D` | delete the next character |
| `^K` | kill to end of line |
| `^Y` | yank (paste) killed text |
| `^O` | open a blank line |
| `^C` | insert a space without moving the cursor |
| `^J` | new line, keeping the current indentation |
| `^Q` | insert the next key literally |
| `M-SPACE` or `M-.` | set the mark |
| `^X ^X` | swap the cursor and the mark |
| `^S` | search forward (end the pattern with `ESC`) |
| `^L` | redraw the screen |
| `^X ^F` | find (open) a file |
| `^X ^R` | read a file into the current buffer |
| `^X ^S` or `^X S` | save the current buffer |
| `^X ^W` | write the buffer to a new file name |
| `^X N` | change the buffer's file name |
| `^X D` | directory listing |
| `^X B` | switch to a named buffer |
| `^X X` | next buffer |
| `^X K` | kill a buffer |
| `^X =` | show the cursor position |
| `M-?` | help |
| `^G` | cancel the current command |
| `^X ^C` or `M-Z` | exit (asks first if there are unsaved changes) |

Numeric arguments work as in Emacs: `^U` alone repeats the next command
four times, and `^U 8 ^N` or `ESC 8 ^N` moves down eight lines.

## Differences from MicroEMACS 3.6

To fit in 48 KB, this port leaves out window splitting, keyboard macros,
query-replace, region commands, paragraph filling, the `EMACS.RC`
startup file and shell commands. The terminal driver
([src/atari.c](src/atari.c)) draws on the 40x24 GRAPHICS 0 screen
through cc65's `conio`.

To save heap, a buffer with no unsaved changes is freed when you switch
away from it and read back from disk, with the cursor at the top, when
you return. You can therefore keep more files open than fit in memory at
once.

## Repository layout

```
src/           editor source (C89 for cc65) and linker config
data/          files staged onto the boot disk (help, welcome)
tests/         unit, integration and stress tests
mk/            build rules included by the top-level Makefile
tools/         build helper scripts
doc/           design notes
third_party/   Atari DOS 2.5 (see its README)
```

## License

The original MicroEMACS by Dave Conroy, with later work by Steve Wilhite,
George Jones and Daniel Lawrence, is in the public domain. The Atari
port, build system and tests are released under the [MIT License](LICENSE).
[Atari DOS 2.5](third_party/atari-dos25/README.md) is not covered by
either license; it is © Atari and is included only so the boot disk can be
built. See [NOTICE](NOTICE) for details.
