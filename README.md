# easy_terminal 1.0

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
| Several jobs on one line | `make & ls -a` |
| Conditional chaining | `make && ./run` — the right side runs only if the left one succeeded |
| Signal handling | `Ctrl+C` interrupts every stage of the pipeline, not the shell |
| Built-in `cd` | `cd ..` |

## Build

```sh
make sources/easy_terminal
./sources/easy_terminal
```

Requires a C compiler and a POSIX system. Builds clean with `-Wall -Werror`;
tested with gcc on Linux and clang on macOS.

Operators do not need spaces around them: `ls>out.txt`, `ls|sort` and
`make&&./run` parse the same as their spaced forms. Two-character operators are
only recognised when written together — `a > > b` is a single `>`, not `>>`.

## Repository layout

```
sources/     easy_terminal.c — the shell
examples/    demo_pipe.c — one pipe between two commands
             reader.c — read a command line and execute it
             catcher.c — catches SIGINT instead of dying, used by test.sh
test.ch      a handful of command lines used to exercise the parser by hand
test.sh      builds everything and checks that Ctrl+C reaches the child
```

`demo_pipe` and `reader` are the pieces the shell grew out of, kept as the
smallest working illustration of each mechanism. Build them the same way:
`make examples/demo_pipe`.

## Tests

```sh
sh test.sh
```

Builds every target with `-Wall -Werror`, then checks signal handling: that
`examples/catcher` survives `SIGINT` on its own, that `Ctrl+C` sent to the shell
reaches the running child while the shell itself stays alive, and that the
program still works at `-O0`, `-O2` and `-O3`. Exits non-zero if anything fails.
Verified on macOS (clang) and Ubuntu 26.04 (gcc 15).
