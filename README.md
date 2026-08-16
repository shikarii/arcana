# Arcana

Arcana is a programming language whose canonical source representation is a
topology-aware semantic graph — drawn magic circles — rather than conventional
text. The compiler operates on discrete topological facts (nodes, directed
edges, region containment, cyclic port order) and lowers them through a
standard pipeline to stack-based bytecode executed by a small VM.

This is an early-stage project. The v0 prototype implements a compiler,
bytecode VM, verifier, reference interpreter, and CLI toolchain in C17.
It can compile and run programs with functions, branching, loops, and
recursion (`fib(10) == 55`).

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

The test suite covers bytecode encoding, VM execution, semantic graph
validation, compiler passes, and end-to-end programs. There are currently
46 tests.

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
  compiler/         Semantic graph → bytecode compilation, diagnostics
  bytecode/         Opcode definitions, .mgc binary format, disassembler
  vm/               Stack-based VM, tagged values, call frames
  verifier/         Independent bytecode verification
  interpreter/      Reference interpreter (direct graph evaluation)
tools/              CLI tool sources
tests/              Test suite and fixtures
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
string), 256 call frames, and a 1024-slot operand stack.

The **reference interpreter** directly evaluates the semantic graph without
compiling, used for differential testing against the compiler+VM pipeline.

## Bytecode format

`.mgc` files use a custom binary format:

- Magic bytes: `ARCA`
- Version: major.minor (currently 0.1)
- Constant pool (tagged values)
- Function table (arity, locals, stack depth, code offset, debug name)
- Code section (27 opcodes)
- Debug section (optional, bytecode offset → source element ID)

All opcodes are defined once in `src/bytecode/opcodes.h` using an X-macro
pattern. The VM, verifier, assembler, and disassembler derive from this
single source of truth.

## Current status

The v0 prototype is functional. The following milestones are complete:

- Bytecode vocabulary and binary format
- Assembler, disassembler, verifier
- Stack VM with tagged values
- Semantic graph library and validator
- Topology-aware lowering (containment, edges, cyclic port order)
- Compiler pipeline (graph → bytecode)
- Functions, calls, returns
- Branching (if/else)
- Loops and recursion
- String values (refcounted)
- Global variables
- Structured diagnostics
- Reference interpreter
- CI on Linux, Windows, and macOS

Not yet implemented: type system, closures, garbage collection, modules,
generics, drawing editor, or any visual frontend.

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
