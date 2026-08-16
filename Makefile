# Convenience Makefile — wraps CMake for common operations.
# Works on Linux, macOS, and Windows (Git Bash / MSYS2).

BUILD_DIR   ?= build
BUILD_TYPE  ?= Debug
CMAKE       ?= cmake
CTEST       ?= ctest

.PHONY: all configure build test clean cli run-tests help

all: build

# --- Configure -----------------------------------------------------------

configure:
	$(CMAKE) -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

# --- Build ----------------------------------------------------------------

build: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(BUILD_TYPE) -j4

# --- Test -----------------------------------------------------------------

test: build
	$(CTEST) --test-dir $(BUILD_DIR) -C $(BUILD_TYPE) --output-on-failure

run-tests: test

# --- Clean ----------------------------------------------------------------

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean 2>/dev/null || true
	rm -rf $(BUILD_DIR)

# --- Release build --------------------------------------------------------

release:
	$(MAKE) BUILD_TYPE=Release build

release-test:
	$(MAKE) BUILD_TYPE=Release test

# --- Help -----------------------------------------------------------------

help:
	@echo "Arcana build targets:"
	@echo "  make              Configure and build (Debug)"
	@echo "  make test         Build and run test suite"
	@echo "  make release      Build in Release mode"
	@echo "  make release-test Build and test in Release mode"
	@echo "  make clean        Remove build directory"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_DIR=dir     Build directory (default: build)"
	@echo "  BUILD_TYPE=type   Debug or Release (default: Debug)"
