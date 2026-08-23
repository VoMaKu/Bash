# easy_terminal

A minimal Unix shell written in C for a systems programming course. It parses a
command line, forks, and executes — supporting pipes, redirections, background
jobs and signal handling.

## Features

| Feature | Example |
|---|---|
| Command execution | `ls -la` |
| Pipes (arbitrary length) | `ls \| sort \| head` |
| Output redirection | `ls > file.txt` |
| Append mode | `ls >> file.txt` |
| Input redirection | `sort < input.txt > output.txt` |
| Background jobs | `sleep 10 &` |
| Signal handling | `Ctrl+C` interrupts the child, not the shell |
| Built-in `cd` | `cd ..` |

## Build

The `Makefile` holds a single pattern rule, so any version builds by name:

```sh
make sources/easy_terminal_v9      # latest
./sources/easy_terminal_v9
```

Requires a C compiler and a POSIX system. Tested with gcc on Linux.

## Repository layout

```
sources/     easy_terminal_v1.c … v9.c — incremental versions, v9 is current
bin/         prebuilt binaries (v7, v8)
examples/    demo_pipe.c, reader.c — small programs to test pipes and stdin
```

The nine versions are kept on purpose: each adds one capability on top of the
previous one, so the sequence reads as the history of the assignment. Start from
`v1` (execute one command) if you want to follow it; use `v9` if you just want
the shell.

## Known issues

Building with a recent clang fails under `-Werror`:

- `easy_terminal_v9.c:435` and `:446` — loop counter `i` is set but never used.
- `easy_terminal_v9.c:478` — `sleep(0.5)` passes a `double` to a parameter of
  type `unsigned int`, so the intended half-second pause becomes `sleep(0)` and
  does not happen at all. Should be `usleep(500000)`.

These are real defects, not just warnings, and are not fixed yet.
