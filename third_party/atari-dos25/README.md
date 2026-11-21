# Atari DOS 2.5

`DOS.SYS` and `DUP.SYS` are the file management system and menu program
of Atari DOS 2.5, copyright Atari.

They are **not** covered by this project's MIT License. They are included
only so that `make atr` and `make test` work immediately after cloning:
the editor and its tests boot from a real DOS 2.5 disk image. You are
expected to own a licensed copy of Atari DOS 2.5. If you are the
copyright holder and want these files removed, please open an issue.

## Using your own copy

Point `DOS25_DIR` at a directory containing your `DOS.SYS` and `DUP.SYS`:

```
make atr DOS25_DIR=~/atari/dos25
```

The build checks both files against [SHA256SUMS](SHA256SUMS). This
matters because the editor is linked to load at `$1E00`, just above the
`$1CFC` MEMLO of this exact DOS build in its default configuration. A
DOS configured with more drive or file buffers uses more low memory, and
the editor would overwrite it. If you know your DOS is compatible, skip
the check with `DOS25_VERIFY=0`.
