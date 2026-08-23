Small standalone programs the shell grew out of. Both build and run on their own.

demo_pipe   reads a line "command | command" and runs it through a single pipe.
            The shell generalises this to a chain of any length.
reader      reads a command line, splits it into words and executes it, one line
            at a time, until end of input. No pipes, no redirections.

Build with the Makefile in the repository root:
    make examples/demo_pipe
    make examples/reader
