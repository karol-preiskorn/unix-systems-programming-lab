# Unix Systems Programming Examples

A small collection of C programs for exploring Unix process behavior and
interprocess communication. The examples cover `fork()`, inherited process
attributes, environment variables, file descriptors, and named pipes (FIFOs).

## Programs

| Program | Demonstrates |
| --- | --- |
| `fifo_pipe` | Communication between a collector and one or more writers through a named FIFO |
| `fork_env_z1` | Process attributes inherited or changed after `fork()` |
| `example-1` | Independent parent and child copies of an in-memory counter |

## Requirements

- A Unix-like operating system with POSIX APIs
- A C99 compiler such as GCC or Clang
- GNU Make
- AddressSanitizer and UndefinedBehaviorSanitizer support for `make sanitize`
- Doxygen only when regenerating the optional API documentation

## Quick Start

Build every program and run the test sequence:

```bash
make
make test
```

Executables are created in `bin/`:

```text
bin/example-1
bin/fifo_pipe
bin/fork_env_z1
```

Run all examples with descriptive headings:

```bash
make run
```

## FIFO Communication

`fifo_pipe` uses `temp.fifo` in the current directory. With no options, the
program starts its own collector process, sends ten timestamped messages, waits
for the collector to finish, and removes the FIFO:

```bash
./bin/fifo_pipe
```

Example output:

```text
#0 PID 12345: 2026-09-12 10:15:00
#1 PID 12345: 2026-09-12 10:15:00
...
#9 PID 12345: 2026-09-12 10:15:00
```

The reader and writer can also run independently. Start the collector first:

```bash
# Terminal 1
./bin/fifo_pipe -r

# Terminal 2
./bin/fifo_pipe -w
```

Start additional `-w` processes to test multiple producers. Stop the collector
with `Ctrl+C`; its signal handler closes and removes `temp.fifo`.

```text
Usage: ./bin/fifo_pipe [-r | -w | -h]
  -r  collect and display messages from the FIFO
  -w  write ten messages to an existing collector
  -h  display this help
```

Each record is smaller than `PIPE_BUF` and is sent with one `write()` call.
POSIX therefore guarantees that records from concurrent writers are not
interleaved. The collector opens the FIFO for reading and writing, allowing it
to remain active when no external writer is connected. A standalone writer
retries briefly while waiting for a collector and then exits with an error.

## Process Inheritance

Run the detailed process-attribute example with:

```bash
./bin/fork_env_z1
```

The program prints values before `fork()`, in the child, and in the parent after
the child exits. It demonstrates:

- new PID and PPID relationships in the child;
- inherited user, group, and session identifiers;
- inherited working directory and file-creation mask;
- an inherited open descriptor for `/dev/null`;
- inherited environment data, including `FORK_DEMO`;
- inherited open-file resource limits; and
- independent copies of global and automatic variables.

The child increments its copies of the global and automatic values. The parent
retains the original values, illustrating the separate address spaces created
by `fork()`.

## Minimal Fork Example

Run the counter example with:

```bash
./bin/example-1
```

Both processes increment their own copy of the same counter from 1 through 5.
Scheduling is controlled by the operating system, so parent and child output
ordering can vary between runs.

## Make Targets

| Target | Purpose |
| --- | --- |
| `make` or `make all` | Build every executable |
| `make check` | Run compiler syntax and warning checks without linking |
| `make test` | Build and execute all three examples |
| `make run` | Run all examples with descriptive headings |
| `make run-fifo` | Build and run the self-contained FIFO example |
| `make run-fork` | Build and run the process-inheritance example |
| `make run-example` | Build and run the minimal counter example |
| `make sanitize` | Clean, rebuild, and test with ASan and UBSan |
| `make rebuild` | Remove binaries and build everything again |
| `make clean` | Remove binaries, object files, and `temp.fifo` |
| `make clean-all` | Also remove generated Doxygen HTML and LaTeX output |
| `make info` | List detected sources and resulting executables |
| `make help` | Display the primary targets |
| `make verbose` | Build with verbose compiler output |
| `make install` | Install executables under `PREFIX/bin` |
| `make uninstall` | Remove installed executables from `PREFIX/bin` |

Build variables can be overridden on the command line:

```bash
make CC=clang CFLAGS="-Wall -Wextra -std=c99 -O2"
```

## Installation

The default installation prefix is `/usr/local`, which may require elevated
permissions:

```bash
make install
make uninstall
```

Install under a user-owned prefix instead:

```bash
make install PREFIX="$HOME/.local"
```

Packaging tools can stage files by setting `DESTDIR` separately from the final
prefix:

```bash
make install DESTDIR=/tmp/package-root PREFIX=/usr
make uninstall DESTDIR=/tmp/package-root PREFIX=/usr
```

## Documentation

The repository includes a Doxygen configuration file and generated HTML/LaTeX
output. Regenerate it with:

```bash
doxygen doxy.Doxyfile
```

`make clean-all` removes generated documentation but preserves
`doxy.Doxyfile`.

## Project Layout

```text
.
|-- Makefile
|-- README.md
|-- doxy.Doxyfile
|-- src/
|   |-- example-1.c
|   |-- fifo_pipe.c
|   `-- fork_env_z1.c
`-- bin/                 # generated by make
```

The source wildcard in the Makefile creates one executable for each `src/*.c`
file. Adding another standalone C source file is therefore enough to include it
in subsequent builds.

## Remaining Exercises

The process-inheritance example implements the first exercise in the original
assignment. Two natural extensions are:

1. Demonstrate which process attributes survive an `exec()` call.
2. Accept a user name and inspect `/proc` to list that user's process IDs and
   command names.
