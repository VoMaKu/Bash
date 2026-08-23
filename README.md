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
| Signal handling | `Ctrl+C` interrupts every stage of the pipeline, not the shell |
| Built-in `cd` | `cd ..` |

## Build

```sh
make sources/easy_terminal
./sources/easy_terminal
```

Requires a C compiler and a POSIX system. Builds clean with `-Wall -Werror`;
tested with gcc on Linux and clang on macOS.

## Repository layout

```
sources/     easy_terminal.c — the shell
examples/    demo_pipe.c — one pipe between two commands
             reader.c — read a command line and execute it
test.ch      a handful of command lines used to exercise the parser by hand
```

The two programs in `examples/` are the pieces the shell grew out of, kept as
the smallest working illustration of each mechanism. Build them the same way:
`make examples/demo_pipe`.
