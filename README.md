# Arcana

Arcana is a programming language whose canonical source representation is a
topology-aware semantic graph — drawn magic circles — rather than conventional
text. The compiler operates on discrete topological facts (nodes, directed
edges, region containment, cyclic port order) and lowers them through a
multi-stage pipeline (HIR → MIR → bytecode) to a stack-based VM with
mark-sweep garbage collection.

This is an early-stage project implemented in C17. The prototype compiles
and runs programs with functions, branching, loops, recursion, closures,
exceptions, strings, arrays, maps, bitwise ops, type casts, and 9 intrinsics
(`fib(10) == 55`).

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

Requirements: CMake 3.16+, a C17 compiler (GCC, Clang, or MSVC), Python 3.

```bash
cmake -B build -S .
cmake --build build
```

## Testing

```bash
cmake --build build
ctest --test-dir build -C Debug -V
```

CTest runs two tests:
1. **arcana_tests** — 123 unit/integration tests across 10 per-module test files
2. **check_limits** — LOC enforcement (600 lines/file, 60 lines/function)

CI runs on Linux, Windows (MSVC), macOS, and with sanitizers (ASan + UBSan).

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
  common/           Typed IDs, result types, memory helpers, arena allocator
  semantic_graph/   Topology-aware program graph, validation, fixture parser
  semantic/         Graph → HIR lowering, scope resolution, error recovery
  hir/              High-level IR (expression trees, resolved variables)
  mir/              Mid-level IR (basic blocks, terminaries, SSA-like temps)
  typecheck/        Type inference and checking pass
  compiler/         HIR/Graph → bytecode compilation, diagnostics
  bytecode/         41 opcodes (X-macro), .mgc binary format, disassembler
  runtime/          Unified object model (string/array/map/closure/record), GC
  vm/               Stack-based VM, tagged values, call frames, 9 intrinsics
  verifier/         Independent bytecode verification
  interpreter/      Reference interpreter (direct graph evaluation)
  platform/         Platform abstraction (file I/O, clock, strdup)
tools/              CLI tool sources, LOC checker
tests/              10 per-module test files + driver
docs/               Architecture, engineering standards, design guardrails
benchmarks/         Compile benchmark
```

## Architecture

```
Arcana Semantic Graph (nodes, edges, regions, ports)
        │
        ▼
   Semantic Analysis (scope resolution, error recovery)
        │
        ▼
   HIR (expression trees, resolved variables, HIR_POISON)
        │
        ▼
   MIR (basic blocks, temporaries, terminators)
        │
        ▼
   Bytecode Emission (.mgc binary format)
        │
   ┌────┴────┐
   ▼         ▼
Verifier    VM (stack-based, mark-sweep GC)
```

The **semantic graph** is the canonical compiler input. Nodes represent
operations, edges carry values between ports, and regions define scope
boundaries. Cyclic port order on each node determines operand evaluation
order — this is what makes the representation geometry-native rather than
a conventional AST.

The **compiler** walks the graph, resolves symbols, lowers through HIR/MIR,
and emits bytecode with constant pool entries, function records (including
upvalue descriptors for closures), and debug metadata mapping bytecode ranges
back to source element IDs.

The **verifier** independently validates bytecode before execution: opcode
validity, operand bounds, stack height consistency at control-flow join
points, and termination.

The **VM** is a stack machine with tagged values (null, bool, i64, f64, obj),
256 call frames, a 1024-slot operand stack, and mark-sweep garbage collection.
Six heap object types: string, array, map, closure, upvalue, record.

The **reference interpreter** directly evaluates the semantic graph without
compiling, used for differential testing against the compiler+VM pipeline.

## Bytecode format

`.mgc` files use a custom binary format:

- Magic bytes: `ARCA`
- Version: major.minor (currently 0.1)
- Constant pool (tagged values with deduplication)
- Function table (arity, locals, stack depth, code offset, upvalue descriptors)
- Code section (41 opcodes)
- Debug section (optional, bytecode offset → source element ID)

All opcodes are defined once in `src/bytecode/opcodes.h` using an X-macro
pattern. The VM, verifier, assembler, and disassembler derive from this
single source of truth.

## Current status

The prototype is feature-complete for v0 language semantics:

- 41 bytecode opcodes + 9 intrinsics
- Arithmetic (i64/f64), comparisons, branching, loops, recursion
- Functions, closures with Lua-style upvalues
- Strings (immutable, concat, slice, index, len)
- Arrays (dynamic, push/index/length) and maps (hash table, keys)
- Records (named field structs)
- Exception handling (try/catch/throw with frame unwinding)
- Bitwise operations (and/or/xor/not/shift)
- Type casts (cast_i64, cast_f64, cast_str)
- Short-circuit booleans (and/or)
- Mark-sweep garbage collector with stress mode
- Type checker pass
- Error recovery via HIR_POISON (multi-diagnostic reporting)
- Structured diagnostics with stable ARC-XXX-NNNN codes
- 123 tests across 10 per-module files, LOC enforcement via CTest
- CI on Linux, Windows, macOS + sanitizers (all green)

Not yet implemented: classes/objects, module/import system, drawing editor,
visual frontend, topology-derived runtime semantics (parallel regions,
reactive execution, effect boundaries).

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
- **LOC limits** — 600 lines per file, 60 lines per function, enforced by CI.
- **Lower-away rule** — compile geometry away when it can be fully resolved
  at compile time; give it a runtime primitive when it cannot.

## CI

GitHub Actions runs on every push and pull request:
- Ubuntu (GCC)
- Windows (MSVC)
- macOS (Clang)
- Sanitizers (ASan + UBSan)

## License

Not yet determined.
