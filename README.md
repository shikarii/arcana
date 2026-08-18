# Arcana

Arcana is a programming language whose canonical source representation is a
topology-aware semantic graph — drawn magic circles — rather than conventional
text. The compiler operates on discrete topological facts (nodes, directed
edges, region containment, cyclic port order) and lowers them through a
standard pipeline to stack-based bytecode executed by a small VM.

The v0 prototype implements a compiler, bytecode VM (66 opcodes),
mark-sweep GC, verifier, reference interpreter, type checker, and CLI
toolchain in C17. It supports functions, closures, branching, loops,
recursion, arrays, maps, records, exception handling, coroutines,
threads, mutexes, and channels — 192 tests across three challenge
corpus levels (L0 machine sanity, L1 algorithms, L2 concurrency).

| Resource              | Link                                           |
|-----------------------|------------------------------------------------|
| Source code           | https://github.com/shikarii/arcana              |
| Issue tracker         | https://github.com/shikarii/arcana/issues       |

## How topology encodes semantics

In conventional languages, meaning comes from textual syntax — indentation,
operator precedence, left-to-right reading order. In Arcana, meaning comes
from graph topology:

| Topology              | Semantic meaning         |
|-----------------------|--------------------------|
| Region containment    | Lexical scope            |
| Directed edges        | Value/data dependencies  |
| Cyclic port order     | Operand ordering         |

The compiler receives an `ArcGraph` containing these facts and lowers them to
bytecode. There is no text parser. Textual formats (assembly, test fixtures)
exist only as engineering tools.

## Build instructions

Requirements: CMake 3.16+, a C17 compiler (GCC, Clang, or MSVC).

```bash
cmake -B build -S .
cmake --build build
```

## Testing

```bash
cmake --build build
ctest --test-dir build -V
```

192 tests covering bytecode encoding, VM execution, semantic graph
validation, compiler passes, GC, closures, concurrency, and end-to-end
programs across three challenge corpus levels.

## CLI tools

| Tool              | Description                                        |
|-------------------|----------------------------------------------------|
| `arcana-run`      | Load and execute `.mgc` bytecode                   |
| `arcana-dis`      | Disassemble `.mgc` to human-readable output        |
| `arcana-asm`      | Assemble textual bytecode (`.asm`) to `.mgc`       |
| `arcana-verify`   | Verify bytecode safety (stack, jumps, control flow) |
| `arcana-compile`  | Compile `.graph` fixture files to `.mgc`            |

Example:

```bash
./build/arcana-asm tests/hello.asm -o hello.mgc
./build/arcana-verify hello.mgc
./build/arcana-run hello.mgc
```

## Project layout

```
src/
  common/           Typed IDs, result types, memory helpers
  semantic_graph/   Topology-aware program graph, validation, fixture parser
  semantic/         HIR/MIR lowering and semantic analysis
  compiler/         Semantic graph → bytecode compilation, constant folding
  bytecode/         Opcode definitions (X-macro), .mgc binary format, disassembler
  vm/               Stack-based VM, tagged values, call frames, concurrency
  runtime/          Object model, mark-sweep GC, concurrency primitives
  platform/         Cross-platform threading (pthreads / Win32)
  verifier/         Independent bytecode verification
  interpreter/      Reference interpreter (direct graph evaluation)
  typecheck/        Static type checking
  service/          Tooling API (parse → compile → verify → run)
tools/              CLI tool sources
tests/              Test suite, fixtures, L0/L1/L2 challenge corpus
docs/               Architecture decisions, engineering standards
```

## Architecture

```
Arcana Semantic Graph (nodes, edges, regions, ports)
        │
        ▼
   Compiler (topology-aware lowering)
        │
        ▼
   Bytecode (.mgc binary format)
        │
   ┌────┴────┐
   ▼         ▼
Verifier    VM (stack-based execution)
```

The **semantic graph** is the canonical compiler input. Nodes represent
operations, edges carry values between ports, and regions define scope
boundaries. Cyclic port order on each node determines operand evaluation
order — this is what makes the representation geometry-native rather than
a conventional AST.

The **compiler** walks the graph, resolves symbols, and emits bytecode with
constant pool entries, function records, and optional debug metadata mapping
bytecode ranges back to source element IDs.

The **verifier** independently validates bytecode before execution: opcode
validity, operand bounds, stack height consistency at control-flow join
points, and termination (every function ends with halt or return).

The **VM** is a stack machine with tagged values (null, bool, i64, f64,
obj), 256 call frames, and a 1024-slot operand stack. Heap objects
(strings, arrays, maps, closures, records, coroutines, threads, mutexes,
channels) are managed by a mark-sweep garbage collector with thread-safe
allocation.

The **reference interpreter** directly evaluates the semantic graph without
compiling, used for differential testing against the compiler+VM pipeline.

## Bytecode format

`.mgc` files use a custom binary format:

- Magic bytes: `ARCA`
- Version: major.minor (currently 0.1)
- Constant pool (tagged values)
- Function table (arity, locals, stack depth, code offset, debug name)
- Code section (66 opcodes)
- Debug section (optional, bytecode offset → source element ID)

All opcodes are defined once in `src/bytecode/opcodes.h` using an X-macro
pattern. The VM, verifier, assembler, and disassembler derive from this
single source of truth.

## Current status

The v0 prototype is feature-complete for its target scope. Complete:

- 66-opcode bytecode vocabulary and `.mgc` binary format
- Assembler, disassembler, verifier
- Stack VM with tagged values and mark-sweep GC
- Semantic graph library, validator, HIR/MIR lowering
- Topology-aware lowering (containment, edges, cyclic port order)
- Compiler pipeline with constant folding and short-circuit evaluation
- Functions, closures (Lua-style upvalues), calls, returns
- Branching (if/else), loops, recursion, topology-derived cycles
- Strings, arrays, maps, records
- Type casts (i64, f64, string), bitwise operations
- Exception handling (try/catch/throw)
- Coroutines (cooperative multitasking)
- OS threads, mutexes, channels (preemptive concurrency)
- 9 intrinsics (print, clock, type, assert, tostring, input, len, push, keys)
- Static type checker
- Tooling service API
- Structured diagnostics with stable error codes
- Reference interpreter for differential testing
- Three-level challenge corpus (L0/L1/L2): 36 fixture-based tests
- CI on Linux, Windows, and macOS

Not yet implemented: modules, generics, drawing editor, or any visual
frontend.

## Contributing

Development uses feature branches and pull requests against `main`.

Key rules:

- **Deterministic compilation** — same graph always produces identical
  bytecode. No hash-map iteration order or timestamps in output.
- **Layer separation** — the VM does not import compiler internals; the
  compiler does not import VM internals.
- **Single source of truth** — opcodes defined once; consumers derive.
- **Stable element IDs** — every semantic element retains identity through
  the pipeline into debug metadata.

## CI

GitHub Actions runs on every push and pull request, building and testing
on Ubuntu, Windows (MSVC), and macOS.

## License

Not yet determined.
