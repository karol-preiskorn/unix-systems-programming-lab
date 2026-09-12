# Makefile for Unix Systems Programming Project
# This project demonstrates Unix/Linux system programming concepts including:
# - FIFO (Named Pipes)
# - Process Management (fork, exec)
# - Environment Variables

CC ?= gcc
CPPFLAGS ?=
CFLAGS ?= -Wall -Wextra -std=c99 -g
LDFLAGS ?=
LDLIBS ?=
PREFIX ?= /usr/local
DESTDIR ?=
INSTALL ?= install
SRCDIR := src
BINDIR := bin
DOCDIR := html

# Source files
SOURCES := $(wildcard $(SRCDIR)/*.c)
# Generate executable names from source files
EXECUTABLES := $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%,$(SOURCES))

# Default target
.PHONY: all
all: $(EXECUTABLES)

# Create bin directory if it doesn't exist
$(BINDIR):
	@mkdir -p $(BINDIR)

# Compile each C source file into an executable
$(BINDIR)/%: $(SRCDIR)/%.c Makefile | $(BINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)
	@echo "✓ Built $@"

# Run all programs
.PHONY: run
run: all
	@echo "📌 Running FIFO example..."
	@$(BINDIR)/fifo_pipe
	@echo "📌 Running fork environment test..."
	@$(BINDIR)/fork_env_z1
	@echo "📌 Running example-1..."
	@$(BINDIR)/example-1

# Run FIFO pipe program
.PHONY: run-fifo
run-fifo: $(BINDIR)/fifo_pipe
	@echo "📌 Running FIFO example..."
	@$<

# Run fork environment test
.PHONY: run-fork
run-fork: $(BINDIR)/fork_env_z1
	@echo "📌 Running fork environment test..."
	@$<

# Run example-1 program
.PHONY: run-example
run-example: $(BINDIR)/example-1
	@echo "📌 Running example-1..."
	@$<

# Clean build artifacts
.PHONY: clean
clean:
	@rm -rf $(BINDIR)
	@rm -f $(SRCDIR)/*.o
	@rm -f temp.fifo
	@echo "✓ Cleaned build artifacts"

# Clean all including documentation
.PHONY: clean-all
clean-all: clean
	@rm -rf $(DOCDIR) Makefile.bak latex/
	@echo "✓ Cleaned all artifacts including documentation"

# Display help
.PHONY: help
help:
	@echo "Unix Systems Programming - Makefile targets"
	@echo "=========================================="
	@echo ""
	@echo "Build targets:"
	@echo "  make all           - Build all programs (default)"
	@echo "  make check         - Check all C sources without linking"
	@echo "  make test          - Build and run all programs"
	@echo "  make sanitize      - Rebuild and test with ASan and UBSan"
	@echo "  make rebuild       - Clean and build all programs"
	@echo "  make clean         - Remove build artifacts"
	@echo "  make clean-all     - Remove all generated files"
	@echo ""
	@echo "Run targets:"
	@echo "  make run           - Run all example programs"
	@echo "  make run-fifo      - Run FIFO pipe example"
	@echo "  make run-fork      - Run fork environment test"
	@echo "  make run-example   - Run example-1 program"
	@echo ""
	@echo "Info targets:"
	@echo "  make help          - Display this help message"
	@echo "  make info          - Show source and executable files"
	@echo "  make install       - Install programs under PREFIX"
	@echo "  make uninstall     - Remove programs from PREFIX"
	@echo ""
	@echo "Project Programs:"
	@echo "  • fifo_pipe    - Named pipe (FIFO) communication example"
	@echo "  • fork_env_z1  - Fork process and environment variables test"
	@echo "  • example-1    - Basic example program"

# Display project information
.PHONY: info
info:
	@echo "📚 Project Information"
	@echo "======================="
	@echo "Source directory: $(SRCDIR)"
	@echo "Build directory:  $(BINDIR)"
	@echo ""
	@echo "Source files:"
	@for src in $(SOURCES); do echo "  • $$src"; done
	@echo ""
	@echo "Executables:"
	@for exe in $(EXECUTABLES); do echo "  • $$exe"; done

# Rebuild everything
.PHONY: rebuild
rebuild: clean all
	@echo "✓ Rebuild complete"

# Check syntax of all C files without building
.PHONY: check
check:
	@echo "🔍 Checking C source files..."
	@for src in $(SOURCES); do \
		echo "Checking $$src..."; \
		$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $$src || exit 1; \
	done
	@echo "✓ All files passed syntax check"

# Install (copy executables to system location)
.PHONY: install
install: all
	@echo "📦 Installing programs..."
	@$(INSTALL) -d "$(DESTDIR)$(PREFIX)/bin"
	@$(INSTALL) -m 755 $(EXECUTABLES) "$(DESTDIR)$(PREFIX)/bin"
	@echo "✓ Installed programs in $(DESTDIR)$(PREFIX)/bin"

# Uninstall
.PHONY: uninstall
uninstall:
	@echo "🗑️  Uninstalling programs..."
	@for exe in $(EXECUTABLES); do \
		rm -f "$(DESTDIR)$(PREFIX)/bin/$$(basename $$exe)"; \
		echo "✓ Removed $$(basename $$exe)"; \
	done

# Run all programs with detailed output
.PHONY: test
test: all
	@echo "🧪 Running tests..."
	@echo ""
	@echo "Test 1: FIFO communication"
	@echo "=========================="
	@$(BINDIR)/fifo_pipe
	@echo ""
	@echo "Test 2: Fork Environment"
	@echo "========================"
	@$(BINDIR)/fork_env_z1
	@echo ""
	@echo "Test 3: Basic fork example"
	@echo "=========================="
	@$(BINDIR)/example-1
	@echo ""
	@echo "✓ All tests completed"

# Rebuild and run tests with runtime memory and undefined-behavior checks
.PHONY: sanitize
sanitize:
	+@$(MAKE) clean
	+@$(MAKE) \
		CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined" test

# Verbose build - show compilation commands
.PHONY: verbose
verbose: CFLAGS += -v
verbose: all

.PHONY: .SILENT
.SILENT: help info
