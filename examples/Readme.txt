Small standalone programs the shell grew out of. Each builds and runs on its own.

demo_pipe   reads a line "command | command" and runs it through a single pipe.
            The shell generalises this to a chain of any length.
reader      reads a command line, splits it into words and executes it, one line
            at a time, until end of input. No pipes, no redirections.
catcher     catches SIGINT and prints HELLO instead of dying. Run it from the
            shell and press Ctrl+C: the shell must stay alive and catcher must
            print HELLO and exit.

Build with the Makefile in the repository root:
    make examples/demo_pipe
    make examples/reader
    make examples/catcher
