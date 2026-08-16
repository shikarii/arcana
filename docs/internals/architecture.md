# Arcana Architecture Overview

## Compilation pipeline

```
Semantic Graph (ArcGraph)
        |
        v
Semantic Analysis (src/semantic/)
  - Symbol/scope resolution
  - Operand order from cyclic port order
  - Structured diagnostics
        |
        v
HIR (src/hir/)
  - Explicit expression trees
  - Resolved variable indices
  - Functions with typed parameter lists
        |
        v
MIR (src/mir/)
  - Basic blocks with terminators
  - Temporaries (single-assignment)
  - Linearized control flow
        |
        v
Bytecode Emission (src/compiler/)
  - Constant pool generation
  - Function table construction
  - Stack depth tracking
  - Debug metadata
        |
        v
.mgc Binary (src/bytecode/)
  - Versioned container format
  - Serialization / deserialization
        |
   +----+----+
   |         |
   v         v
Verifier   VM Execution
(src/verifier/)  (src/vm/)
```

## Module responsibilities

| Module | Inputs | Outputs | Invariants |
|--------|--------|---------|------------|
| semantic_graph/ | Drawing topology | ArcGraph | IDs unique, parentage acyclic |
| semantic/ | ArcGraph | HirModule | Variables resolved, order explicit |
| hir/ | — | Data structures | Expressions are trees, vars indexed |
| mir/ | HirModule | MirModule | Every block terminates, valid jumps |
| compiler/ | HirModule or ArcGraph | ArcBytecodeImage | Deterministic output |
| bytecode/ | — | .mgc format | Single source of truth for opcodes |
| verifier/ | ArcBytecodeImage | Valid/errors | Stack heights consistent at joins |
| vm/ | Verified bytecode | Execution result | No unsafe memory access |
| interpreter/ | ArcGraph | Result | Reference semantics for testing |
| platform/ | — | OS abstraction | Portable across Win/Linux/macOS |

## Dependency direction

```
common
  ^
  |
diagnostics    platform
  ^               ^
  |               |
semantic_graph ---|
  ^
  |
semantic --> hir
               |
               v
             mir
               |
               v
bytecode <-- compiler
  ^
  |
verifier
  ^
  |
vm
```

Lower layers never import higher layers. The VM does not know about
the compiler. The compiler does not know about the VM.

## Key design decisions

See `docs/adr/` for architecture decision records:
- ADR-0001: C17 as implementation language
- ADR-0002: Geometry-native source model
