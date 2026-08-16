# Arcana

A programming language whose canonical source syntax is a drawn, topology-aware magic-circle representation. C17 implementation.

## Architecture

```
Drawing / Editor           [out of scope]
        |
Topological Normalization  [future frontend]
        |
Arcana Semantic Graph      [compiler input]
        |
Semantic Analysis / HIR
        |
MIR / CFG
        |
Arcana Bytecode (.mgc)
        |
Arcana VM
```

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build -V
```

## Rules

- **C17** with `-Wall -Wextra -Werror`
- **No textual user syntax** — text forms exist only for assembly, debug dumps, test fixtures, and serialization
- **Single source of truth** for opcodes — `src/bytecode/opcodes.h` is canonical; VM, verifier, assembler, and disassembler derive from it
- **Deterministic compilation** — no hash-map iteration order dependencies, no timestamps in bytecode
- **Layer separation** — VM must not import compiler/HIR; compiler must not import VM internals; bytecode definitions are shared
- **Stable source IDs** — every semantic element has a stable ID that survives lowering into debug metadata
- **Tests required** — every milestone adds tests at the correct layer

## Reference Documents

- `docs/ARCANA_REPOSITORY_ENGINEERING_STANDARDS.md`
- `docs/design/arcana_agent_kickoff_prompt.md`
- `docs/adr/` — architecture decision records

## Module Map

| Directory | Purpose |
|-----------|---------|
| `src/common/` | Typed IDs, result types, memory helpers |
| `src/bytecode/` | Opcode enum, instruction encoding, .mgc format |
| `src/vm/` | Stack VM interpreter, values, call frames |
| `src/verifier/` | Independent bytecode verification |
| `src/semantic_graph/` | Topology-aware program graph |
| `src/compiler/` | Semantic graph -> HIR -> MIR -> bytecode |
| `tools/` | arcana-run, arcana-asm, arcana-dis, arcana-verify CLIs |
| `tests/` | Unit, integration, golden, conformance tests |
