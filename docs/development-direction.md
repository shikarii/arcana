# Arcana Development Direction

**Status:** Living document — updated each development cycle
**Date:** 2026-08-16
**Prerequisite reading:** `arcana_compiler_architecture.docx`, `arcana_vm_compiler_runtime_architecture.docx`, `ARCANA_REPOSITORY_ENGINEERING_STANDARDS.md`

---

## 1. Where We Are

### 1.1 Implemented (Milestones A–G complete)

Arcana has a working vertical slice from semantic graph to VM execution:

| Layer | Module | LOC | Status |
|-------|--------|-----|--------|
| Common | `src/common/` | 87 | Stable — typed IDs, status codes, memory macros |
| Semantic Graph | `src/semantic_graph/` | 925 | Complete — nodes, edges, regions, ports, cyclic order, fixture parser, validation |
| Semantic Analysis | `src/semantic/` | 781 | Working — graph→HIR lowering, symbol resolution, scope analysis |
| HIR | `src/hir/` | 662 | Working — expression trees, functions, params, validation, dump |
| MIR | `src/mir/` | 478 | Working — basic blocks, temporaries, terminators, validation, dump |
| Compiler | `src/compiler/` | 784 | Working — graph→bytecode, constant pool, function table, debug maps |
| Bytecode | `src/bytecode/` | 662 | Complete — opcodes, .mgc format, serialization, disassembler |
| Verifier | `src/verifier/` | 202 | Working — stack depth, jump targets, constant indices |
| Runtime | `src/runtime/` | 452 | Complete — unified object model (string/array/map/closure/upvalue), mark-sweep GC |
| VM | `src/vm/` | 971 | Complete — 41 opcodes, string ops, bitwise, type casts, arrays, maps, closures, exceptions, 9 intrinsics |
| Interpreter | `src/interpreter/` | 436 | Working — reference semantics evaluator for differential testing |
| Platform | `src/platform/` | 89 | Working — strdup, file I/O, clock, terminal detection |
| Diagnostics | `src/compiler/diagnostics.h` | (in compiler) | Complete — structured codes (ARC-XXX-NNNN), severity, source refs |

**Total source:** ~7,400 LOC across 20 .h + 19 .c files
**Tests:** 130 passing across 14 test files
**CI:** 4 jobs — Linux, Windows, macOS, Sanitizers (all green)
**CLI tools:** arcana-run, arcana-dis, arcana-verify, arcana-asm, arcana-compile
**Fuzzing:** 2 fuzz targets (bytecode, fixture)
**Benchmarks:** 1 (compile benchmark)
**Linting:** -Wpedantic -Wshadow -Wstrict-prototypes -Werror (GCC/Clang), /W4 /WX (MSVC), .clang-tidy

### 1.2 What Works End-to-End

```
Hand-authored semantic graph fixture
  → Topology validation (regions, ports, edges, cyclic order)
  → Topology-aware lowering (containment→scope, edges→deps, port order→operand order)
  → HIR construction (expression trees, resolved variables)
  → MIR lowering (basic blocks, temporaries, terminators)
  → Bytecode emission (constant pool, function table, debug maps)
  → .mgc serialization
  → Independent verification
  → VM execution
  → Result: 15 from "5 + 10", 55 from fib(10)
```

This proves the core thesis: **drawing-derived topology determines program meaning**, and the result survives through a conventional compiler pipeline to execution.

---

## 2. What the Vision Documents Require

The three source documents (`compiler_architecture.docx`, `vm_runtime_architecture.docx`, `engineering_standards.md`) collectively define a language that:

1. Has its own bytecode format, assembler, disassembler, verifier, and VM ✅
2. Uses geometry-native source semantics (regions, edges, cyclic port order) ✅
3. Preserves stable source element IDs through compilation ✅
4. Has an explicit HIR → MIR → bytecode pipeline ✅
5. Has a reference interpreter for differential testing ✅
6. Supports structured diagnostics with stable error codes ✅
7. Has separate compiler/VM/verifier trust boundaries ✅
8. Is deterministic in compilation ✅
9. Has clean platform abstraction ✅
10. Compiles `5 + 10 = 15` and `fib(10) = 55` ✅

**All 10 foundational requirements are met.** The prototype is proven.

---

## 3. Gaps: What's Missing

### 3.1 Language Features (from vision doc §12, §15)

| Feature | Vision Status | Implementation Status | Priority |
|---------|--------------|----------------------|----------|
| Null, Bool, Int values | Required v0 | ✅ Done | — |
| Float (64-bit) | Required v0 | ✅ Done (VAL_F64, arithmetic, comparisons) | — |
| Immutable String | Required when heap ready | ✅ Done (OBJ_STRING via GC, concat, slice, index, len) | — |
| Local bindings | Required v0 | ✅ Done | — |
| Lexical regions/scopes | Required v0 | ✅ Done | — |
| Functions | Required v0 | ✅ Done | — |
| Calls + return | Required v0 | ✅ Done | — |
| If/else | Required v0 | ✅ Done | — |
| Iteration/loops | Required v0 | ✅ Done (while loop via semantic graph, tested fib/sum) | — |
| Comparisons | Required v0 | ✅ Done | — |
| Basic output (print) | Required v0 | ✅ Done (clock intrinsic) | — |
| Closures/captures | Deferred | ⚠️ VM opcodes exist (closure, get/set_upval, close_upval); upvalue wiring incomplete | MEDIUM |
| Classes/objects | Deferred | ❌ Not implemented | LATER |
| Exceptions | Deferred | ✅ Done (try_begin/try_end/throw, handler stack, frame unwinding) | — |
| Module/import system | Deferred | ❌ Not implemented | LATER |

### 3.2 Runtime & Memory (from VM doc §6, §19)

| Feature | Vision Status | Implementation Status | Priority |
|---------|--------------|----------------------|----------|
| Tagged values (null/bool/int) | Required v0 | ✅ Done | — |
| Float values | Required v0 | ✅ Done (VAL_F64) | — |
| Heap object header | Required for strings | ✅ Done (ArcObject: type, marked, next) | — |
| Immutable UTF-8 strings | Required v0+ | ✅ Done (OBJ_STRING, concat, slice, index) | — |
| Arrays | Required v0+ | ✅ Done (OBJ_ARRAY, array_new, index_get/set, length, push) | — |
| Maps | Required v0+ | ✅ Done (OBJ_MAP, map_new, index_get/set, keys) | — |
| Records/structs | Required v0+ | ✅ Done (OBJ_RECORD, field access by index) | — |
| Mark-sweep GC | Required when heap exists | ✅ Done (mark roots → sweep, allocation threshold trigger) | — |
| GC stress mode | Required by standards | ✅ Done (collect every alloc when enabled) | — |

### 3.3 Compiler Infrastructure (from compiler doc, CPython analysis)

| Feature | Source | Implementation Status | Priority |
|---------|--------|----------------------|----------|
| Arena allocator for compiler | CPython pattern, eng standards §19 | ✅ Done (src/common/arena.h/.c) | — |
| Separated scope resolution pass | Compiler doc §7 | ✅ Done (src/semantic/scope.h/.c) | — |
| Type checking pass | Compiler doc §8 | ✅ Done (src/typecheck/typecheck.h/.c) | — |
| Constant folding | Compiler doc §16.2 | ❌ Missing | LOW |
| Short-circuit boolean lowering | Compiler doc §14.4 | ✅ Done (compiler emits conditional jumps) | — |
| Loop lowering (while, break, continue) | Compiler doc §14.2-14.3 | ✅ Done (while + CYCLE topology-derived loops) | — |
| Closure/capture analysis | Compiler doc §15.3 | ⚠️ VM opcodes exist; compiler capture analysis missing | MEDIUM |
| Error recovery (continue past errors) | Eng standards §6, CPython pattern | ✅ Done (poison nodes, multi-diagnostic) | — |
| Opcode generation from data file | Eng standards §8, CPython pattern | ✅ Done (X-macro in opcodes.h) | — |

### 3.4 Testing & Quality (from eng standards §25-28, CPython analysis)

| Feature | Source | Implementation Status | Priority |
|---------|--------|----------------------|----------|
| Per-subsystem test files | Eng standards §25 | ✅ Done (14 test files: bytecode, vm, gc, graph, pipeline, e2e, infra, etc.) | — |
| Golden tests (disassembly, IR dumps) | Eng standards §25.5 | ✅ Done (golden disassembly in test_pipeline.c) | — |
| Compile-fail tests | Eng standards §25.3 | ✅ Done (test_error_recovery.c) | — |
| Runtime-fail tests | Eng standards §25.4 | ✅ Done (division by zero, stack overflow, unhandled throw) | — |
| Property-based tests | Eng standards §27 | ✅ Done (roundtrip serialization property test) | — |
| Conformance test suite | Eng standards §25.7 | ❌ Missing | LATER |
| Differential testing (interpreter vs VM) | Vision doc §17 | ✅ Done (test_cycle_interp_agreement, test_ref_interp_*) | — |
| 500+ tests before public release | CPython analysis recommendation | ⚠️ At 130 | HIGH |

### 3.5 Documentation (from eng standards §57-58)

| Feature | Source | Status | Priority |
|---------|--------|--------|----------|
| Architecture overview | Eng standards | ✅ `docs/internals/architecture.md` | — |
| Bytecode format spec | Vision doc §27 | ✅ `docs/reference/bytecode-format.md` | — |
| Opcode reference | Vision doc §27 | ✅ `docs/reference/opcode-reference.md` | — |
| Semantic graph spec | Vision doc §27 | ✅ `docs/reference/semantic-graph.md` | — |
| Testing strategy doc | Eng standards | ✅ `docs/internals/testing.md` | — |
| Engineering standards | — | ✅ `docs/ARCANA_REPOSITORY_ENGINEERING_STANDARDS.md` | — |
| Language specification | Eng standards §24 | ❌ Missing | MEDIUM |
| HIR/MIR format docs | Vision doc §27 | ❌ Missing | LOW |
| Contributor guide | Eng standards §58 | ❌ Missing | LOW |

### 3.6 Repository Structure (from eng standards §4)

The engineering standards specify a target structure. Current vs target:

| Directory | Target | Current | Gap |
|-----------|--------|---------|-----|
| `spec/` | Language/bytecode/VM specs | ❌ Missing | Need to create |
| `tests/unit/` | Per-module unit tests | ❌ Single file | Need to split |
| `tests/integration/` | End-to-end tests | ❌ In test_all.c | Need to split |
| `tests/compile-fail/` | Invalid program tests | ❌ Missing | Need to create |
| `tests/golden/` | Golden output tests | ❌ Missing | Need to create |
| `examples/` | Example programs | ❌ Missing | Need to create |
| `fixtures/` | Graph fixtures | ❌ Inline in tests | Need to extract |
| `src/runtime/` | Heap/GC/objects | ✅ Done (object.h/.c, gc.h/.c) | — |
| `src/diagnostics/` | Separated diagnostics | ⚠️ In compiler/ | Consider extraction |

---

## 4. Development Roadmap

### Phase 1: Language Completeness ✅ COMPLETE

**Goal:** Arcana v0 can express all basic programs defined in the vision documents.

#### 1.1 Floating-Point Support ✅
- `VAL_F64` value tag, arithmetic on ints and floats, `OP_CAST_I64`/`OP_CAST_F64` for conversion.

#### 1.2 While Loops ✅
- Semantic graph `REGION_LOOP_BODY`, compiled to `JUMP`/`JUMP_IF_FALSE` bytecode. Tested with `sum_to(10)=55`.

#### 1.3 Short-Circuit Booleans ✅
- `and`/`or` lowered to conditional jumps in compiler (JUMP_IF_FALSE/JUMP_IF_TRUE + DUP/POP).

#### 1.4 Strings + Object Model ✅
- Unified `ArcObject` header (`type`, `marked`, `next`), `OBJ_STRING` (immutable UTF-8).
- String ops: `OP_ADD` concat, `OP_STR_LEN`, `OP_STR_SLICE`, `OP_STR_INDEX`, `OP_CAST_STR`.
- ~450 LOC in `src/runtime/object.h/.c`, `src/vm/value_ops.c`.

#### 1.5 Mark-Sweep Garbage Collector ✅
- Mark roots (stack, globals, open upvalues) → sweep. Allocation threshold trigger.
- Traces into arrays, maps, closures, upvalues.
- ~180 LOC in `src/runtime/gc.h/.c`.

#### 1.6 Collections ✅
- `OBJ_ARRAY` (dynamic, push/index/length), `OBJ_MAP` (key-value, linear probe).
- `OP_ARRAY_NEW`, `OP_MAP_NEW`, `OP_INDEX_GET`, `OP_INDEX_SET`, `OP_LENGTH`.
- Intrinsics: `push` (array append), `keys` (map → array of keys).

#### 1.7 Exception Handling ✅
- Handler stack: `OP_TRY_BEGIN`, `OP_TRY_END`, `OP_THROW`.
- Unwinds call frames, restores stack, jumps to catch IP.

#### 1.8 Bitwise + Type Casts ✅
- Bitwise: `OP_BIT_AND/OR/XOR/NOT`, `OP_SHL`, `OP_SHR` (i64-only).
- Type casts: `OP_CAST_I64`, `OP_CAST_F64`, `OP_CAST_STR`.

#### 1.9 Closure Opcodes ⚠️ Partial
- VM opcodes exist: `OP_CLOSURE`, `OP_GET_UPVAL`, `OP_SET_UPVAL`, `OP_CLOSE_UPVAL`.
- `OBJ_CLOSURE` and `OBJ_UPVALUE` object types defined.
- **Missing**: Upvalue descriptors in `ArcFuncRecord`, compiler capture analysis, full VM wiring.
- **Est:** ~200 LOC across format.h/.c, vm.c, compiler.c

### Phase 2: Compiler Hardening ✅ COMPLETE

#### 2.1 Arena Allocator ✅
- `ArcArena` in `src/common/arena.h/.c` — pool allocator with block chaining.

#### 2.2 Type Checking Pass ✅
- `src/typecheck/typecheck.h/.c` — separated pass, recursive memoized inference, ARC-TYPE-NNNN codes.

#### 2.3 Separated Scope Resolution ✅
- `src/semantic/scope.h/.c` — extracted from semantic.c, `ArcScope` API.

#### 2.4 Error Recovery ✅
- Poison node propagation in semantic analysis. Multi-diagnostic reporting.

### Phase 3: Test Infrastructure ✅ COMPLETE

#### 3.1 Split Test Suite ✅
- 14 test files: test_runtime, test_vm, test_gc, test_bytecode, test_verifier, test_graph, test_pipeline, test_pipeline_e2e, test_infra, test_vm_collections, test_error_recovery, test_cycle.

#### 3.2 Golden Tests ✅
- Disassembly golden test in test_pipeline.c.

#### 3.3 Compile-Fail Tests ✅
- Error recovery tests: multi-error, poison-no-cascade, HIR poison.

#### 3.4 Differential Tests ✅
- Interpreter vs compiler+VM agreement: test_cycle_interp_agreement, test_ref_interp_*.

### Phase 4: Geometry Extensions

After the v0 language features are solid, explore the geometry-native extensions from vision doc §4.2:

#### 4.1 Edge Crossing Semantics
- Crossing two edges = interaction/synchronization point
- Requires graph topology analysis beyond simple connectivity
- Design document first, then implementation

#### 4.2 Region Boundary Crossing
- Edge crossing a region boundary = capture/export/capability transfer
- Natural fit for closure capture analysis
- Connects to compiler doc §15.3

#### 4.3 Directed Cycles as Iteration ✅
- CYCLE regions + BREAK_IF nodes — topology-derived loops.
- Compiler + interpreter both handle CYCLE body scanning for BREAK_IF exit conditions.
- 7 tests: sum variants, interpreter agreement, semantic lowering, scope resolution.

#### 4.4 Concentric Regions as Effect Domains
- Nested typed regions = transaction/effect/lifetime scoping
- Foundation for effect system

### Phase 5: Ecosystem Foundation

#### 5.1 Module System
- File-level compilation units
- Symbol export/import
- Semantic graph references to external circles
- Bytecode linking

#### 5.2 Arrays and Records
- `ObjArray`: resizable `Value[]`
- `ObjRecord`: field array indexed by compiler-assigned offsets
- Member access operations

#### 5.3 Closures
- Capture analysis pass (compiler doc §15.3)
- Upvalue/cell representation
- `ObjClosure` object type

#### 5.4 WASM Target
- Emscripten build of arcana-run
- Browser-based semantic graph visualization
- Natural fit: drawing tool → browser → WASM VM

#### 5.5 Language Specification
- Formal specification document in `spec/`
- Covers all implemented semantics
- Conformance test suite derived from spec

---

## 5. Opcode Generation from Data (Engineering Standards §8)

The engineering standards require a single source of truth for opcodes. Current state: hand-maintained switch statements in disassembler, verifier, VM, and assembler.

**Recommended approach:**
1. Define opcodes in `src/bytecode/opcodes.toml`:
   ```toml
   [CONST_I64]
   code = 0x01
   operands = ["u16:const_index"]
   stack_in = 0
   stack_out = 1
   description = "Push 64-bit integer constant"
   ```
2. Generator script produces:
   - `opcodes_generated.h` — enum + names
   - Disassembler operand format table
   - Verifier stack-effect table
   - Documentation table
3. CI validates generated files are up-to-date

---

## 6. Implementation Order (Prioritized)

This is the recommended order. Each item is a commit-sized unit of work.

```
 ✅  Float support (opcodes, VM, compiler, tests)
 ✅  While loops (graph→HIR→MIR→bytecode, tests)
 ✅  String object type + GC (runtime/, heap objects, mark-sweep)
 ✅  Arrays + maps (runtime collections, VM opcodes)
 ✅  Exception handling (try/catch/throw, handler stack)
 ✅  Bitwise + type casts (6 bitwise ops, 3 cast ops)
 ✅  Extended intrinsics (type, assert, tostring, len, push, keys)
 ✅  Short-circuit booleans (compiler lowering, tests)
 ✅  Compiler integration (all opcodes emitted by compiler)
 ✅  Arena allocator (src/common/arena.h/.c)
 ✅  Split test suite (14 test files, 130 tests)
 ✅  Type checking pass (src/typecheck/)
 ✅  Error recovery (poison nodes, multi-diagnostic)
 ✅  Records/structs (OBJ_RECORD)
 ✅  GC stress mode (collect every alloc)
 ✅  Golden tests + compile-fail tests + differential tests
 ✅  Opcode generation from data file (X-macro in opcodes.h)
 ✅  Directed-cycle-as-loop (CYCLE regions + BREAK_IF, 7 tests)
 ✅  Scope extraction (src/semantic/scope.h/.c)
 ✅  Pedantic linting (-Wpedantic, -Wshadow, /WX, .clang-tidy)
--- above completed ---
1.  Finish closure upvalue wiring (format.h, vm.c, compiler.c)
2.  Edge crossing semantics (design document first)
3.  Module system
4.  Language specification
5.  WASM/Emscripten build
6.  Constant folding
7.  Conformance test suite
```

---

## 7. Quality Gates

Before any public release or external visibility:

- [x] All v0 language features working (float, strings, loops, functions, branches, arrays, maps, exceptions)
- [x] GC with stress mode passing all tests
- [ ] 500+ tests across split test files (at 130)
- [x] Golden tests for disassembly and IR dumps
- [x] Compile-fail test suite
- [x] Differential testing (interpreter vs VM) automated
- [x] Zero sanitizer warnings (ASan, UBSan, LeakSan)
- [x] Zero compiler warnings on GCC, Clang, MSVC with -Wpedantic/-Wshadow/WX (CI green on all 4 jobs)
- [ ] Architecture docs current
- [ ] Engineering standards compliance self-audit
- [x] At least one geometry-native semantic test (CYCLE topology-derived iteration, 7 tests)

---

## 8. Principles (Carried from Vision Documents)

1. **The drawing is the language.** Never accidentally create a text syntax that becomes the "real" language.
2. **Correctness before cleverness.** No optimization until unoptimized behavior is tested.
3. **Determinism.** Same input → same bytecode, same execution.
4. **Each layer owns its semantics.** Lower layers never reach upward.
5. **Structured diagnostics.** Every error has a stable code and source provenance.
6. **The VM is boring.** Let the source language be strange; the VM is the stable substrate.
7. **Build from the bottom up.** Don't wait for the editor; prove the design with the executable stack.
8. **Smallest reversible design.** When uncertain, implement the minimal version that preserves architecture.

---

## 9. Session Protocol

Every development session:

1. Read this document's §3 gaps table — pick the top unfinished item
2. Build and test current state: `cmake -B build && cmake --build build && ctest --test-dir build -V`
3. Implement the feature with tests
4. Update the gaps table in this document (move ❌ → ✅)
5. Commit with descriptive message
6. Push to feature branch, create PR if CI passes
