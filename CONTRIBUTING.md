# Contributing

Thanks for your interest in atari-emacs. Bug reports, fixes and tests are
welcome.

## Before you start

The editor runs in 48 KB with about 7.8 KB of heap for text. Every byte of
code or static data a change adds comes out of that heap. Prefer changes
that keep the binary the same size or make it smaller, and say in your pull
request how the size changed.

## Workflow

1. Build and test on a clean checkout first, so you know the baseline:

   ```
   make deps
   make test
   ```

   Note the `me.xex` size printed by the build, and the heap and coverage
   figures printed by `make test-unit`.

2. Make your change, then run `make test` again. All tests must pass.
   Compare the size and heap figures with your baseline. CI runs the same
   tests on every pull request and shows these figures in the run summary.

3. Add or update tests for any behaviour you change:
   - a unit test in the matching `tests/target/test_<suite>.c`, plus its
     entry in `tests/driver/test_runner_resident.c` (see
     [tests/README.md](tests/README.md#adding-a-new-suite-for-another-source-file));
   - an integration scenario under `tests/integration/scenarios/` for
     anything that involves key sequences or files on disk.

4. Update [CHANGELOG.md](CHANGELOG.md) under an `Unreleased` heading.

## Code style

- `src/` keeps the upstream MicroEMACS style, including its mix of tab
  and space indentation (8-column tab stops), so diffs against upstream
  stay readable. Match the surrounding code, and don't reformat code you
  aren't otherwise changing.
- The code must compile with cc65, which accepts C89 plus a few
  extensions. Avoid floating point, `long` arithmetic in hot paths, and
  large automatic arrays: the C stack is 512 bytes.
- Use `unsigned char` for values that fit in 8 bits; 6502 code for `int`
  is much larger. Make sure loop counters are wide enough for their bounds.
- Tests use four-space indentation.

## Changing the memory layout

`src/atari.cfg`, `tests/target/test-resident.cfg`,
`tests/target/test-overlay.cfg` and `tests/target/abi_table.h` together
fix the addresses that the editor, the test image and the test overlays
rely on. Read [doc/memory-map.md](doc/memory-map.md) and the comments in
those files before changing any of them, and run the full test suite
afterwards.

## Commit messages

Use a short summary line in the imperative ("Fix ffgetline for long
lines"), a blank line, then an explanation of what changed and why,
including size or heap effects where relevant.
