# CPython Project Analysis & Gap Analysis for Arcana

**Date:** 2026-08-16
**Scope:** Full structural analysis of [CPython](https://github.com/python/cpython) (3.16-dev) vs Arcana

---

## 1. CPython Directory Structure

```
cpython/
├── Grammar/                    # Language grammar definition
│   ├── python.gram             #   PEG grammar (the single source of truth)
│   └── Tokens                  #   Token definitions (literals, operators)
│
├── Parser/                     # Frontend: tokenization + parsing
│   ├── Python.asdl             #   ASDL spec → generates AST C structs
│   ├── asdl.py                 #   ASDL parser (Python)
│   ├── asdl_c.py               #   ASDL → C code generator
│   ├── parser.c                #   Generated PEG parser (~220K LOC, machine-gen)
│   ├── pegen.c / pegen.h       #   PEG parser runtime engine
│   ├── pegen_errors.c          #   Parser error recovery & messages
│   ├── peg_api.c               #   Public parser API
│   ├── string_parser.c/.h      #   f-string / string literal parsing
│   ├── token.c                 #   Token type definitions
│   ├── myreadline.c            #   Interactive line input
│   ├── action_helpers.c        #   Semantic actions during parsing
│   ├── lexer/                  #   Lexer implementation
│   └── tokenizer/              #   Tokenizer implementation
│
├── Python/                     # Core interpreter engine (~120 C files)
│   ├── compile.c               #   AST → instruction sequence compiler
│   ├── codegen.c               #   Code generation from AST
│   ├── flowgraph.c             #   Control flow graph + optimizations
│   ├── assemble.c              #   CFG → final bytecode emission
│   ├── symtable.c              #   Symbol table (scope analysis)
│   ├── ast.c                   #   AST construction + validation
│   ├── ast_preprocess.c        #   AST preprocessing passes
│   ├── ast_unparse.c           #   AST → source string (for repr)
│   ├── ceval.c                 #   Main eval loop (the interpreter core)
│   ├── ceval_gil.c             #   GIL management
│   ├── ceval_macros.h          #   Interpreter dispatch macros
│   ├── bytecodes.c             #   Instruction definitions (DSL)
│   ├── generated_cases.c.h     #   Generated from bytecodes.c
│   ├── executor_cases.c.h      #   Generated JIT executor cases
│   ├── jit.c                   #   JIT compiler entry point
│   ├── jit_publish.c           #   JIT code publication
│   ├── jit_unwind.c            #   JIT stack unwinding
│   ├── specialize.c            #   Adaptive specialization
│   ├── optimizer.c             #   Trace optimizer (tier 2)
│   ├── gc.c                    #   Garbage collector (default build)
│   ├── gc_free_threading.c     #   GC for free-threaded build
│   ├── gc_gil.c                #   GC/GIL interaction
│   ├── import.c                #   Module import system
│   ├── marshal.c               #   Object serialization (.pyc files)
│   ├── pylifecycle.c           #   Interpreter init/fini lifecycle
│   ├── pystate.c               #   Thread/interpreter state
│   ├── errors.c                #   Exception handling
│   ├── traceback.c             #   Traceback formatting
│   ├── tracemalloc.c           #   Memory allocation tracing
│   ├── frame.c                 #   Frame object management
│   ├── bltinmodule.c           #   Built-in functions (print, len, etc.)
│   ├── sysmodule.c             #   sys module
│   ├── intrinsics.c            #   Compiler intrinsics
│   ├── instruction_sequence.c  #   Instruction sequence data structure
│   ├── instrumentation.c       #   sys.monitoring implementation
│   ├── dtoa.c                  #   Float ↔ string conversion (David Gay's)
│   ├── pyarena.c               #   Arena memory allocator for compiler
│   ├── pytime.c                #   Time abstraction
│   ├── fileutils.c             #   File utility functions
│   ├── lock.c                  #   Synchronization primitives
│   ├── critical_section.c      #   Critical section implementation
│   ├── qsbr.c                  #   Quiescent-state-based reclamation
│   ├── crossinterp.c           #   Cross-interpreter communication
│   ├── thread.c                #   Thread abstraction
│   ├── thread_nt.h             #     Windows threading
│   ├── thread_pthread.h        #     POSIX threading
│   ├── dynload_shlib.c         #   Dynamic loading (Unix)
│   ├── dynload_win.c           #   Dynamic loading (Windows)
│   ├── emscripten_*.c          #   Emscripten/WASM support
│   ├── asm_trampoline_*.S      #   Arch-specific JIT trampolines (x86_64, aarch64, riscv64)
│   ├── frozen.c                #   Frozen module support
│   ├── frozen_modules/         #   Pre-compiled frozen modules
│   └── clinic/                 #   Argument Clinic generated code
│
├── Objects/                    # Object type implementations (~60 C files)
│   ├── object.c                #   Base object protocol (refcount, repr, hash)
│   ├── typeobject.c            #   Type system (metaclass, MRO, descriptors)
│   ├── longobject.c            #   Arbitrary-precision integers
│   ├── floatobject.c           #   IEEE 754 floats
│   ├── complexobject.c         #   Complex numbers
│   ├── boolobject.c            #   Boolean type
│   ├── unicodeobject.c         #   Unicode strings (~15K LOC, largest single file)
│   ├── bytesobject.c           #   Bytes objects
│   ├── bytearrayobject.c       #   Mutable byte arrays
│   ├── listobject.c            #   List implementation (dynamic array)
│   ├── tupleobject.c           #   Tuple (immutable sequence)
│   ├── dictobject.c            #   Dict (hash table, compact + ordered)
│   ├── setobject.c             #   Set/frozenset
│   ├── funcobject.c            #   Function objects
│   ├── codeobject.c            #   Code objects (compiled bytecode)
│   ├── frameobject.c           #   Frame objects (execution context)
│   ├── genobject.c             #   Generators/coroutines/async generators
│   ├── iterobject.c            #   Iterator protocol
│   ├── moduleobject.c          #   Module objects
│   ├── descrobject.c           #   Descriptors (properties, slots)
│   ├── classobject.c           #   Instance methods
│   ├── methodobject.c          #   Built-in method wrappers
│   ├── capsule.c               #   PyCapsule (opaque C pointer wrapper)
│   ├── exceptions.c            #   Exception hierarchy
│   ├── call.c                  #   Unified calling convention
│   ├── abstract.c              #   Abstract object interface (PyNumber, PySequence)
│   ├── obmalloc.c              #   Custom memory allocator (pymalloc)
│   ├── sliceobject.c           #   Slice objects
│   ├── rangeobject.c           #   Range iterator
│   ├── memoryobject.c          #   Buffer protocol / memoryview
│   ├── structseq.c             #   Named tuples from C
│   ├── weakrefobject.c         #   Weak references
│   ├── namespaceobject.c       #   SimpleNamespace
│   ├── odictobject.c           #   OrderedDict C implementation
│   ├── lazyimportobject.c      #   Lazy imports (3.16+)
│   ├── interpolationobject.c   #   Template string interpolation (3.14+)
│   ├── templateobject.c        #   Template objects
│   ├── sentinelobject.c        #   Sentinel values
│   ├── unionobject.c           #   Union type (X | Y)
│   ├── typevarobject.c         #   TypeVar, ParamSpec, TypeVarTuple
│   ├── genericaliasobject.c    #   Generic aliases (list[int])
│   ├── stringlib/              #   String algorithm library (shared)
│   ├── mimalloc/               #   mimalloc allocator integration
│   ├── clinic/                 #   Argument Clinic generated wrappers
│   └── dictnotes.txt           #   Dict implementation design notes
│
├── Modules/                    # Extension modules (~100+ C files)
│   ├── main.c                  #   Main entry point
│   ├── gcmodule.c              #   gc module interface
│   ├── posixmodule.c           #   os module (POSIX)
│   ├── signalmodule.c          #   signal module
│   ├── _io/                    #   I/O library (buffered, text, file)
│   ├── _sqlite/                #   sqlite3 module
│   ├── _ssl/                   #   SSL/TLS module
│   ├── _ctypes/                #   Foreign function interface
│   ├── _decimal/               #   Decimal arithmetic
│   ├── _sre/                   #   Regular expression engine
│   ├── _hacl/                  #   HACL* verified crypto
│   ├── _testcapi/              #   C API test suite
│   ├── _testinternalcapi/      #   Internal C API tests
│   ├── _testlimitedcapi/       #   Limited C API tests
│   ├── _remote_debugging/      #   Remote debugging support
│   ├── mathmodule.c            #   math module
│   ├── cmathmodule.c           #   cmath module
│   ├── socketmodule.c          #   socket module
│   ├── selectmodule.c          #   select/poll/epoll
│   ├── timemodule.c            #   time module
│   ├── itertoolsmodule.c       #   itertools module
│   ├── _asynciomodule.c        #   asyncio C accelerators
│   ├── _json.c                 #   json C scanner/encoder
│   ├── _pickle.c               #   pickle C implementation
│   ├── unicodedata.c           #   Unicode database
│   ├── zlibmodule.c            #   zlib compression
│   ├── _bz2module.c            #   bz2 compression
│   ├── _lzmamodule.c           #   lzma compression
│   ├── mmapmodule.c            #   Memory-mapped files
│   ├── fcntlmodule.c           #   File control (Unix)
│   ├── _winapi.c               #   Windows API bindings
│   ├── overlapped.c            #   Windows overlapped I/O
│   ├── getpath.c               #   Path computation
│   ├── Setup.stdlib.in         #   Module build configuration
│   └── expat/                  #   Embedded expat XML parser
│
├── Programs/                   # Entry points
│   ├── python.c                #   main() for python executable
│   ├── _bootstrap_python.c     #   Minimal bootstrap interpreter
│   ├── _freeze_module.c        #   Freeze modules into C
│   └── _testembed.c            #   Embedding API tests
│
├── Include/                    # Public + internal headers
│   ├── Python.h                #   Master include (public API)
│   ├── object.h                #   PyObject struct, refcount macros
│   ├── pyport.h                #   Portability definitions
│   ├── pymem.h                 #   Memory allocation API
│   ├── cpython/                #   CPython-specific headers (~60 files)
│   │   ├── object.h            #     Extended object internals
│   │   ├── pyatomic_gcc.h      #     GCC atomics
│   │   ├── pyatomic_msc.h      #     MSVC atomics
│   │   ├── pyatomic_std.h      #     C11 atomics
│   │   └── ...
│   └── internal/               #   Private internal headers (~140 files)
│       ├── pycore_ceval.h      #     Eval loop internals
│       ├── pycore_gc.h         #     GC internals
│       ├── pycore_runtime.h    #     Runtime state
│       ├── pycore_interp.h     #     Interpreter state
│       ├── pycore_frame.h      #     Frame internals
│       ├── pycore_compile.h    #     Compiler internals
│       ├── pycore_optimizer.h  #     JIT optimizer types
│       ├── pycore_jit.h        #     JIT compiler internals
│       ├── pycore_flowgraph.h  #     CFG internals
│       ├── pycore_symtable.h   #     Symbol table internals
│       ├── pycore_code.h       #     Code object internals
│       ├── pycore_dict.h       #     Dict internals
│       ├── pycore_stackref.h   #     Stack references (free-threading)
│       └── ...
│
├── Lib/                        # Standard library (Python)
│   ├── test/                   #   Test suite (434 test_*.py files)
│   │   ├── libregrtest/        #     Test runner framework
│   │   └── support/            #     Test utilities
│   ├── asyncio/                #   Async I/O framework
│   ├── collections/            #   Container datatypes
│   ├── concurrent/             #   concurrent.futures
│   ├── compression/            #   Compression modules
│   ├── importlib/              #   Import system
│   ├── unittest/               #   Unit test framework
│   ├── _pyrepl/                #   Interactive REPL
│   └── ...                     #   ~200+ stdlib modules
│
├── PC/                         # Windows-specific
│   ├── pyconfig.h              #   Windows pyconfig (hand-maintained)
│   ├── WinMain.c               #   pythonw.exe entry point
│   ├── dl_nt.c                 #   DLL loading
│   ├── msvcrtmodule.c          #   MSVC runtime module
│   ├── winreg.c                #   Windows registry module
│   ├── winsound.c              #   Windows audio
│   ├── config.c                #   Module init table
│   ├── venvlauncher.c          #   venv launcher
│   ├── icons/                  #   Application icons
│   ├── layout/                 #   Directory layout
│   ├── clinic/                 #   Argument Clinic generated code
│   └── validate_ucrtbase.py    #   UCRT validation
│
├── PCbuild/                    # Windows build system (MSBuild/VS)
│   ├── pcbuild.sln             #   Visual Studio solution
│   ├── pythoncore.vcxproj      #   Core library project
│   ├── python.vcxproj          #   python.exe project
│   ├── _ssl.vcxproj            #   Per-module .vcxproj files
│   └── ...                     #   ~80 .vcxproj + .filters files
│
├── Mac/                        # macOS-specific
│   ├── Makefile.in             #   macOS build integration
│   ├── PythonLauncher/         #   macOS .app launcher
│   ├── BuildScript/            #   Framework build scripts
│   ├── IDLE/                   #   macOS IDLE integration
│   ├── Resources/              #   .plist files
│   └── Tools/                  #   macOS-specific tools
│
├── Platforms/                  # Cross-platform targets
│   ├── Android/                #   Android NDK support
│   │   ├── android-env.sh      #     Environment setup
│   │   ├── testbed/            #     Android test harness
│   │   └── __main__.py         #     Build entry point
│   ├── Apple/                  #   iOS/macOS universal
│   │   ├── __main__.py         #     Build entry point
│   │   ├── iOS/                #     iOS-specific
│   │   └── testbed/            #     iOS test harness
│   ├── WASI/                   #   WebAssembly System Interface
│   │   ├── __main__.py         #     Build entry point
│   │   ├── config.site-*       #     Cross-compile config
│   │   └── wasmtime.toml       #     WASI runtime config
│   └── emscripten/             #   Browser/Node WASM
│       ├── __main__.py         #     Build entry point
│       ├── config.site-*       #     Cross-compile config
│       ├── node_entry.mjs      #     Node.js entry point
│       ├── web_example/        #     Browser demo
│       └── browser_test/       #     Browser test runner
│
├── Tools/                      # Development tools
│   ├── build/                  #   Build system utilities
│   │   ├── smelly.py           #     Symbol naming checker
│   │   ├── stable_abi.py       #     Stable ABI checker
│   │   ├── freeze_modules.py   #     Freeze stdlib modules
│   │   ├── deepfreeze.py       #     Deep-freeze objects to C
│   │   ├── generate_*.py       #     Various code generators
│   │   └── check_warnings.py   #     Warning checker
│   ├── c-analyzer/             #   C code static analyzer
│   │   ├── c-analyzer.py       #     Entry point
│   │   ├── c_parser/           #     C parsing library
│   │   ├── c_analyzer/         #     Analysis rules
│   │   └── cpython/            #     CPython-specific rules
│   ├── cases_generator/        #   Bytecode DSL → C case generator
│   ├── clinic/                 #   Argument Clinic (docstring → C wrapper gen)
│   ├── jit/                    #   JIT compiler tools
│   │   ├── build.py            #     JIT build driver
│   │   ├── _stencils.py        #     Code stencil handling
│   │   ├── _targets.py         #     Target architecture configs
│   │   └── template.c          #     JIT template
│   ├── peg_generator/          #   PEG parser generator
│   ├── gdb/                    #   GDB debugging extensions
│   ├── i18n/                   #   Internationalization tools
│   ├── wasm/                   #   WASM build helpers
│   ├── tsan/                   #   ThreadSanitizer suppression files
│   ├── ubsan/                  #   UBSanitizer suppression files
│   ├── scripts/                #   Miscellaneous scripts
│   └── unicode/                #   Unicode table generators
│
├── InternalDocs/               # Developer-facing internal documentation
│   ├── compiler.md             #   Compiler pipeline walkthrough
│   ├── parser.md               #   PEG parser design
│   ├── interpreter.md          #   Bytecode interpreter internals
│   ├── garbage_collector.md    #   GC design (reference counting + cyclic GC)
│   ├── jit.md                  #   JIT architecture (trace recording + copy-and-patch)
│   ├── frames.md               #   Frame layout
│   ├── code_objects.md         #   Code object structure
│   ├── exception_handling.md   #   Exception table format
│   ├── generators.md           #   Generator/coroutine implementation
│   ├── string_interning.md     #   String interning strategy
│   └── ...
│
├── Doc/                        # User-facing documentation (Sphinx)
│   ├── reference/              #   Language reference
│   ├── library/                #   Standard library docs
│   ├── c-api/                  #   C API reference
│   ├── tutorial/               #   Python tutorial
│   ├── howto/                  #   How-to guides
│   ├── extending/              #   Extension module docs
│   ├── whatsnew/               #   What's new per version
│   └── deprecations/           #   Deprecation notices
│
├── Misc/                       # Miscellaneous
│   ├── ACKS                    #   Contributors acknowledgments
│   ├── NEWS.d/                 #   News fragments (Towncrier-style)
│   ├── stable_abi.toml         #   Stable ABI definition
│   ├── valgrind-python.supp    #   Valgrind suppressions
│   ├── sbom.spdx.json          #   Software bill of materials
│   ├── python.man              #   Man page
│   └── mypy/                   #   Mypy type stubs for tools
│
├── .github/workflows/          # CI configuration (25 workflow files)
│   ├── build.yml               #   Main test workflow (reusable)
│   ├── jit.yml                 #   JIT-specific CI
│   ├── lint.yml                #   Linting
│   ├── mypy.yml                #   Type checking
│   ├── tail-call.yml           #   Tail-call interpreter tests
│   ├── reusable-ubuntu.yml     #   Ubuntu build matrix
│   ├── reusable-windows.yml    #   Windows build matrix
│   ├── reusable-macos.yml      #   macOS build matrix
│   ├── reusable-san.yml        #   Sanitizers (ASan, UBSan, TSan)
│   ├── reusable-emscripten.yml #   Emscripten/WASM CI
│   ├── reusable-wasi.yml       #   WASI CI
│   └── ...
│
├── configure.ac                # Autoconf build (POSIX) — Python 3.16
├── configure                   # Generated configure script
├── Makefile.pre.in             # Makefile template
├── pyconfig.h.in               # Config header template
├── setup.py                    # Module discovery (legacy)
└── pyproject.toml              # Modern project metadata
```

---

## 2. CPython C Systems — Deep Insights

### 2.1 Compilation Pipeline

CPython's pipeline has **5 major stages**:

```
Source text
  → Lexer/Tokenizer (Parser/lexer/, Parser/tokenizer/)
    → PEG Parser (Parser/parser.c → AST)
      → Symbol Table (Python/symtable.c)
        → Compiler (Python/compile.c → instruction sequences)
          → Flow Graph (Python/flowgraph.c → CFG optimization)
            → Assembler (Python/assemble.c → bytecode)
              → Code Object (Objects/codeobject.c)
```

**Key design decisions:**
- PEG parser replaced LL(1) in Python 3.9 (PEP 617). Grammar is in `Grammar/python.gram`.
- AST is defined via ASDL (`Parser/Python.asdl`) and auto-generated to C structs.
- Compiler uses an **arena allocator** (`Python/pyarena.c`) — one free call deallocates everything.
- Symbol table pass runs **before** compilation to resolve scopes (local/global/free/cell).
- Flow graph pass does basic block optimization, dead code elimination, constant folding.

### 2.2 Bytecode & Interpreter

- Instructions are 16-bit code units: 8-bit opcode + 8-bit oparg.
- `EXTENDED_ARG` prefix for larger operands (up to 32-bit).
- Main eval loop in `Python/ceval.c` uses computed goto (GCC/Clang) or switch.
- **Adaptive specialization** (PEP 659): bytecodes self-specialize based on runtime types.
- Bytecodes defined in a custom **DSL** (`Python/bytecodes.c`) processed by `Tools/cases_generator/`.

### 2.3 JIT Compiler (3.13+)

- **Copy-and-patch** JIT — precompiled code stencils patched at runtime.
- Two tiers: adaptive interpreter (tier 1) and JIT (tier 2).
- Trace recording: hot loops trigger recording → micro-op (uop) sequence → optimization → native code.
- Architecture-specific trampolines in assembly (`asm_trampoline_x86_64.S`, etc.).
- Build tooling in `Tools/jit/` (Python scripts using LLVM).

### 2.4 Object System

- All objects derive from `PyObject` with `ob_refcnt` + `ob_type`.
- **Reference counting** as primary memory management + cyclic GC for cycle-breaking.
- Type system is fully runtime: `typeobject.c` implements MRO, descriptors, `__slots__`.
- Custom allocator `pymalloc` (`Objects/obmalloc.c`) + mimalloc integration.
- Free lists for common types (ints, floats, tuples, lists).

### 2.5 Memory Management

- Three allocator levels: raw (malloc), object (pymalloc), arena.
- Compiler uses arena allocation — simple, fast, single dealloc.
- `tracemalloc` module for allocation tracing/debugging.
- Mimalloc integrated as alternative allocator.

### 2.6 Threading Model

- **GIL** (Global Interpreter Lock) in default build — one thread executes Python at a time.
- **Free-threaded build** (3.13+, PEP 703): no GIL, uses per-object locks + critical sections.
- QSBR (Quiescent-State-Based Reclamation) for safe memory reclamation.
- Biased reference counting in free-threaded build.
- Platform thread abstraction: `thread_nt.h` (Windows), `thread_pthread.h` (POSIX).

---

## 3. Cross-Platform Support

### 3.1 Build Systems

| Platform | Build System | Config |
|----------|-------------|--------|
| Linux/macOS/BSDs | autoconf + make | `configure.ac` → `configure` → `Makefile` |
| Windows | MSBuild (Visual Studio) | `PCbuild/*.vcxproj` + `PCbuild/pcbuild.sln` |
| Android | Cross-compile via autoconf | `Platforms/Android/` + NDK |
| iOS | Cross-compile via autoconf | `Platforms/Apple/` |
| Emscripten (WASM) | Cross-compile via autoconf | `Platforms/emscripten/` + Emscripten SDK |
| WASI | Cross-compile via autoconf | `Platforms/WASI/` + wasmtime |

**Key takeaway**: CPython does **not** use CMake. POSIX uses autoconf (53K+ line `configure.ac`), Windows uses MSBuild. This is a historical artifact — CMake wasn't mature when CPython's build was designed.

### 3.2 Platform Abstraction Patterns

- **`pyconfig.h`**: Generated by `configure` on POSIX; hand-maintained `PC/pyconfig.h` on Windows.
- **Dynamic loading**: `dynload_shlib.c` (dlopen), `dynload_win.c` (LoadLibrary), `dynload_stub.c` (no dynload).
- **Thread abstraction**: `thread_pthread.h` vs `thread_nt.h`, selected at compile time.
- **Atomics**: `cpython/pyatomic_gcc.h`, `pyatomic_msc.h`, `pyatomic_std.h` — three implementations.
- **File operations**: `Python/fileutils.c` abstracts path encoding, file descriptors, stat.
- **Signal handling**: `Modules/signalmodule.c` with platform-specific signal sets.

### 3.3 Platform-Specific Code Isolation

CPython keeps platform-specific code contained:
- `PC/` — Windows-only C files and config.
- `Mac/` — macOS-specific launcher, framework build.
- `Platforms/` — Newer cross-compile targets (Android, iOS, Emscripten, WASI).
- `#ifdef` usage is scattered throughout core C files (hundreds of instances), but major platform differences are isolated to specific files.

---

## 4. Testing Infrastructure

### 4.1 Scale

- **434 test files** in `Lib/test/` (Python tests for the language and stdlib).
- **3 dedicated C API test modules**: `_testcapi/`, `_testinternalcapi/`, `_testlimitedcapi/`.
- **Custom test runner**: `Lib/test/libregrtest/` — handles parallelism, timeouts, resource management, platform skips.
- **Not using pytest** — CPython uses its own `unittest`-based framework with custom extensions.

### 4.2 CI Matrix

CPython's CI tests on **10+ platform combinations**:
- Ubuntu (multiple versions, multiple compilers)
- Windows (multiple VS versions)
- macOS (multiple architectures)
- Sanitizers: ASan, UBSan, TSan (separate workflow)
- Emscripten (Node.js, browser)
- WASI (wasmtime)
- Free-threaded builds (separate matrix dimension)
- JIT builds (separate workflow)
- Tail-call interpreter (separate workflow)

### 4.3 Quality Tools

- **Argument Clinic** (`Tools/clinic/`): Generates C argument parsing + docstrings from a custom DSL.
- **C static analyzer** (`Tools/c-analyzer/`): Finds global variable issues, naming violations.
- **smelly.py** (`Tools/build/smelly.py`): Checks exported symbol naming conventions.
- **stable_abi.py** (`Tools/build/stable_abi.py`): Validates stable ABI compatibility.
- **Code generators**: ~15 Python scripts that generate C code from data files.
- **Ruff** (`.ruff.toml`): Python linting.
- **mypy**: Type checking for Python tooling.
- **CIFuzz**: Fuzzing integration via OSS-Fuzz.

---

## 5. Documentation

### 5.1 User-Facing (`Doc/`)

- **Sphinx-based** documentation with custom theme.
- Sections: tutorial, language reference, library reference, C API, how-to guides, extending, what's new.
- Per-version "What's New" documents tracking all changes.
- Deprecation tracking in `Doc/deprecations/`.

### 5.2 Developer-Facing (`InternalDocs/`)

- Detailed design documents for each major subsystem.
- Covers: compiler, parser, interpreter, GC, JIT, frames, code objects, exception handling, generators, string interning.
- Written in markdown, living in the repo alongside the code.

### 5.3 Change Management

- **NEWS.d/**: Towncrier-style news fragments — one file per change, merged at release.
- **ACKS**: Contributor acknowledgments.
- **SBOM**: Software Bill of Materials for supply chain security.
- **PEPs** (external): Python Enhancement Proposals for major changes.

---

## 6. Arcana Current State (for comparison)

### 6.1 Scale

| Metric | Arcana | CPython |
|--------|--------|---------|
| Source LOC (C) | ~7,500 | ~600,000+ |
| Test files | 10 per-module + driver | 434 + 3 C test modules |
| Test count | 123 | ~50,000+ |
| Header files | ~22 | ~260+ |
| CLI tools | 5 | 4 |
| Platform targets | 3 (Linux, Windows, macOS) | 7+ (Linux, Windows, macOS, Android, iOS, WASM, WASI) |
| Build system | CMake | autoconf + MSBuild |
| CI workflows | 1 | 25 |
| CI jobs | 4 | 30+ |
| Internal docs | 7 | 18 |
| Fuzzing | 2 fuzz targets | OSS-Fuzz integration |

### 6.2 Architecture Comparison

| Aspect | Arcana | CPython |
|--------|--------|---------|
| Frontend | Semantic graph (geometry) | PEG parser (text grammar) |
| AST/IR | HIR → MIR → Bytecode | AST → instruction seq → CFG → bytecode |
| Diagnostics | Structured codes (ARC-XXX-NNNN) | Ad-hoc error messages |
| Memory | Arena allocator for compiler (`src/common/arena.c`) | Arena allocator for compiler |
| GC | Mark-sweep GC (`runtime/gc.c`) | Refcounting + cyclic GC |
| JIT | None | Copy-and-patch JIT |
| Threads | N/A | GIL + free-threaded option |
| Type system | Static (`src/typecheck/typecheck.c`) | Dynamic (runtime) |
| Module system | None yet | Full import system |
| Object model | Unified `ArcObject` (string, array, map, closure, upvalue) | Fully implemented |
| Opcodes | 41 (string, bitwise, cast, collections, closures, exceptions) + 9 intrinsics | ~120 specialized |
| Exception handling | Handler stack (try/catch/throw) | Exception table + block stack |
| Collections | Arrays + maps + records (runtime) | list, dict, set, tuple (built-in types) |
| Error recovery | HIR_POISON propagation (multi-diagnostic) | Parser error recovery + continue-past-error |
| LOC enforcement | 600/file, 60/function (CTest) | No formal limit |

---

## 7. Gap Analysis & Recommendations

### 7.1 Critical Gaps (Address Before 1.0)

#### G1: Arena Allocator for Compiler Passes ✅ DONE
**CPython does this**: `pyarena.c` — all compiler memory goes into an arena, freed with one call.
**Arcana status**: ✅ Implemented in `src/common/arena.h/.c`. Arena allocator used for compiler temporaries.
**Priority**: Resolved.

#### G2: Symbol Table / Scope Resolution Pass
**CPython does this**: `symtable.c` — dedicated pass that resolves all scopes (local, global, free, cell) before compilation.
**Arcana status**: `semantic.c` does some of this inline during lowering. Not separated.
**Recommendation**: Extract a dedicated scope resolution pass that runs on the semantic graph before HIR lowering. This is required for closures, nested functions, and proper variable capture.
**Priority**: HIGH — blocks closure implementation.

#### G3: Module / Import System
**CPython does this**: `import.c` + `importlib/` — multi-file programs, namespaces, packages.
**Arcana status**: Single-file only. No import mechanism.
**Recommendation**: Design a basic module system. At minimum: file-level compilation units, symbol export/import, linking phase. The semantic graph model (circles reference other circles) maps naturally to this.
**Priority**: HIGH — essential for any non-trivial program.

#### G4: Object Model & Type System ✅ MOSTLY DONE
**CPython does this**: Full runtime type system with `PyObject`, type hierarchy, protocols.
**Arcana status**: ✅ Unified `ArcObject` model with common header (`type`, `marked`, `next`). Six heap types: `OBJ_STRING`, `OBJ_ARRAY`, `OBJ_MAP`, `OBJ_CLOSURE`, `OBJ_UPVALUE`, `OBJ_RECORD`. Mark-sweep GC with stress mode in `runtime/gc.c`. 41 opcodes + 9 intrinsics. Type checker pass in `src/typecheck/typecheck.c`.
**Remaining**: Class definitions, prototype/vtable dispatch, method dispatch.

### 7.2 Important Gaps (Address Before Public Release)

#### G5: Test Infrastructure Scaling ✅ MOSTLY DONE
**CPython does this**: 434 test files, custom runner with parallelism, per-subsystem isolation.
**Arcana status**: ✅ 10 per-module test files with 123 tests. Split by subsystem (bytecode, vm, vm_collections, gc, graph, pipeline, pipeline_e2e, infra, runtime, error_recovery, verifier). LOC enforcement (600/file, 60/function) via CTest.
**Remaining**: Target 500+ tests before public release.

#### G6: Error Recovery in Semantic Analysis ✅ DONE
**CPython does this**: Parser has sophisticated error recovery; compiler continues past errors to report multiple diagnostics.
**Arcana status**: ✅ Implemented via `HIR_POISON` nodes. When semantic analysis encounters an error, it returns a poison node instead of NULL and continues. Poison propagation prevents cascade errors. Tests verify multi-error reporting and poison behavior.
**Priority**: Resolved.

#### G7: Cross-Platform Target Expansion
**CPython does this**: 7+ platforms with dedicated CI for each.
**Arcana status**: CI covers Linux, Windows, macOS + sanitizers (4 jobs). No WASM or mobile targets.
**Recommendation**:
- Add Emscripten CI job (arcana should run in the browser given its geometry-native nature).
- WASM is a natural fit — drawing tools are often web-based.
- Consider Android support early since the drawing paradigm works well on tablets.
**Priority**: MEDIUM — the drawing-based model practically demands browser support.

#### G8: Code Generation from Specifications
**CPython does this**: ASDL → C structs; bytecodes.c DSL → switch cases; Argument Clinic → wrappers. ~15 code generators.
**Arcana status**: All structures hand-written. Opcodes manually maintained.
**Recommendation**:
- Define opcodes in a data file (TOML/CSV) and generate `opcodes.h` + disassembler cases + VM switch.
- Define diagnostic codes in a data file and generate the enum.
- This prevents desynchronization between components.
**Priority**: MEDIUM.

#### G9: Bytecode Versioning & Compatibility
**CPython does this**: Magic number in `.pyc` files, bumped on any bytecode change. Old `.pyc` files are rejected and recompiled.
**Arcana status**: `.mgc` format has some versioning but it's unclear how backwards compatibility is handled.
**Recommendation**: Establish a versioning policy now. Once users exist, bytecode format changes become expensive.
**Priority**: MEDIUM.

### 7.3 Nice-to-Have Gaps (Long Term)

#### G10: Performance Profiling Infrastructure
**CPython does this**: `_lsprof` module, `sys.monitoring`, `pystats.c` compile-time statistics, perf trampoline integration.
**Arcana status**: One benchmark file (`benchmarks/bench_compile.c`).
**Recommendation**: Add compile-time stats (nodes processed, time per pass) and VM execution counters. Essential for optimization work.

#### G11: Debug Information
**CPython does this**: Line number tables, column offsets, exception tables with ranges, `traceback.c`.
**Arcana status**: Some debug metadata support exists in bytecode format. Source mapping back to drawing elements is unique to Arcana.
**Recommendation**: The element_id → bytecode range mapping is Arcana's equivalent of line numbers. Make sure this survives all compiler passes.

#### G12: Argument Clinic Equivalent
**CPython does this**: DSL that generates C wrapper code for argument parsing + docstrings.
**Arcana status**: N/A (no FFI/extension API yet).
**Recommendation**: When Arcana has a C API, consider generating binding code from declarations rather than hand-writing it.

#### G13: Supply Chain Security
**CPython does this**: SBOM (`sbom.spdx.json`), dependency tracking, vendored dependency auditing.
**Arcana status**: No third-party dependencies (pure C17). No SBOM.
**Recommendation**: Good for now since there are no dependencies. Add SBOM tracking when external dependencies are introduced.

---

## 8. What Arcana Does Better

Not all gaps favor CPython. Arcana has some clear advantages:

| Area | Arcana Advantage |
|------|-----------------|
| **Diagnostic system** | Structured codes (ARC-XXX-NNNN) with source refs, related refs, notes — more structured than CPython's ad-hoc messages |
| **Build system** | Single CMake build for all platforms — simpler than autoconf + MSBuild split |
| **IR design** | Explicit HIR → MIR pipeline with validation at each stage. CPython's compiler is more monolithic |
| **Engineering standards** | 2,381-line normative standards document from day one. CPython developed these organically over 30+ years |
| **Platform isolation** | Clean `platform.h/c` abstraction from the start. CPython has `#ifdef` scattered through core files |
| **CI with sanitizers** | Sanitizers in CI from the beginning. CPython added ASan CI much later |
| **Fuzzing** | Fuzz targets from early development. CPython's fuzzing was added later |
| **Type system** | Static typing planned from the start. CPython retrofitted type hints as optional annotations |

---

## 9. Recommended Priority Roadmap

Based on the analysis:

### Phase 1: Foundation (Next 3-6 months)
1. **Arena allocator** — eliminate all compiler leak classes
2. **Object model** — define value types, heap objects, type representation
3. **Scope resolution** — separate pass for variable resolution

### Phase 2: Completeness (6-12 months)
4. **Module system** — file-level compilation, symbol linking
5. **Error recovery** — multi-diagnostic reporting
6. **Test expansion** — split tests, target 500+ test cases
7. **Emscripten/WASM CI** — natural fit for drawing-based language

### Phase 3: Maturity (12-18 months)
8. **Code generation from specs** — opcodes, diagnostics from data files
9. **Bytecode versioning policy** — before public release
10. **Profiling/debug infrastructure** — compile stats, source mapping

### Phase 4: Ecosystem (18+ months)
11. **Standard library** — basic builtins, I/O, math
12. **C extension API** — for FFI/native modules
13. **Package system** — if the language grows a community

---

## 10. Summary

CPython is a 35-year-old project with ~600K+ lines of C and enormous institutional knowledge baked into its structure. Arcana at ~5.6K LOC is in its infancy, but has several structural advantages from being designed with modern engineering practices.

The most impactful things Arcana can learn from CPython:
1. **Arena allocation** for compiler internals (eliminates entire categories of memory bugs)
2. **Separated scope analysis** before code generation
3. **Code generation from specifications** (prevents component desynchronization)
4. **Per-subsystem test isolation** (prevents test file bloat)
5. **Platform abstraction completeness** (especially WASM — critical for a drawing-based language)

The most impactful things Arcana should NOT copy from CPython:
1. The autoconf + MSBuild split (CMake is better)
2. Ad-hoc diagnostic messages (Arcana's structured system is superior)
3. The GIL (design for parallelism from the start)
4. Monolithic compiler (Arcana's HIR → MIR pipeline is cleaner)
