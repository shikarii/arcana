# Arcana Development Direction

**Status:** Living document — updated each development cycle
**Date:** 2026-08-16
**Prerequisite reading:** `arcana_compiler_architecture.docx`, `arcana_vm_compiler_runtime_architecture.docx`, `ARCANA_REPOSITORY_ENGINEERING_STANDARDS.md`, `docs/design/vm-guardrails.md`

---

## 1. Where We Are

### 1.1 Implemented (Milestones A–G + VM completeness + compiler completeness + test infrastructure)

Arcana has a working vertical slice from semantic graph to VM execution:

| Layer | Module | LOC | Status |
|-------|--------|-----|--------|
| Common | `src/common/` | ~90 | Stable — typed IDs, status codes, memory macros, arena allocator |
| Semantic Graph | `src/semantic_graph/` | ~925 | Complete — nodes, edges, regions, ports, cyclic order, fixture parser, validation |
| Semantic Analysis | `src/semantic/` | ~700 | Working — graph→HIR lowering, symbol resolution, scope analysis, error recovery (poison nodes) |
| HIR | `src/hir/` | ~660 | Working — expression trees, functions, params, validation, dump, HIR_POISON |
| MIR | `src/mir/` | ~480 | Working — basic blocks, temporaries, terminators, validation, dump |
| Type Checker | `src/typecheck/` | ~300 | Working — arithmetic, comparison, bitwise, string, cast, collection type inference |
| Compiler | `src/compiler/` | ~575 | Working — graph→bytecode, all 41 opcodes emitted, short-circuit booleans, try/throw |
| Bytecode | `src/bytecode/` | ~660 | Complete — 41 opcodes (X-macro), .mgc format, serialization, disassembler |
| Verifier | `src/verifier/` | ~200 | Working — stack depth, jump targets, constant indices |
| Runtime | `src/runtime/` | ~530 | Complete — unified object model (string/array/map/closure/upvalue/record), mark-sweep GC |
| VM | `src/vm/` | ~900 | Complete — 41 opcodes, string ops, bitwise, type casts, arrays, maps, closures, exceptions, 9 intrinsics |
| Interpreter | `src/interpreter/` | ~440 | Working — reference semantics evaluator for differential testing |
| Platform | `src/platform/` | ~90 | Working — strdup, file I/O, clock, terminal detection |
| Diagnostics | `src/compiler/diagnostics.h` | (in compiler) | Complete — structured codes (ARC-XXX-NNNN), severity, source refs |

**Total source:** ~7,500 LOC across ~22 .h + ~20 .c files
**Tests:** 123 passing across 10 per-module test files
**CI:** 4 jobs — Linux, Windows, macOS, Sanitizers (all green)
**LOC enforcement:** 600 lines/file, 60 lines/function — CTest `check_limits` enforced
**CLI tools:** arcana-run, arcana-dis, arcana-verify, arcana-asm, arcana-compile
**Fuzzing:** 2 fuzz targets (bytecode, fixture)
**Benchmarks:** 1 (compile benchmark)

### 1.2 What Works End-to-End

```
Hand-authored semantic graph fixture
  → Topology validation (regions, ports, edges, cyclic order)
  → Topology-aware lowering (containment→scope, edges→deps, port order→operand order)
  → HIR construction (expression trees, resolved variables, poison error recovery)
  → MIR lowering (basic blocks, temporaries, terminators)
  → Bytecode emission (constant pool, function table, debug maps)
  → .mgc serialization
  → Independent verification
  → VM execution
  → Result: 15 from "5 + 10", 55 from fib(10)
```

This proves the core thesis: **drawing-derived topology determines program meaning**, and the result survives through a conventional compiler pipeline to execution.

### 1.3 VM Maturity Stage (per guardrails doc §12)

**Current: Stage 1 — Semantic Independence**

The VM executes conventional semantics correctly with its own type/value/error/function model. It does not mirror Python behavior — it has its own value representation (tagged union with 5 heap types), its own GC (mark-sweep), its own bytecode format (.mgc), its own exception mechanism (handler stack), and its own calling convention.

**Next milestone: Stage 2 — Topology-derived semantics.** Important language constructs should be inferred from regions, dependencies, cycles, boundaries, or ordering rather than represented only as ordinary AST node labels.

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
| Basic output (print) | Required v0 | ✅ Done (print + clock intrinsics) | — |
| Short-circuit booleans | Required v0 | ✅ Done (and/or lower to JUMP_IF_FALSE/JUMP_IF_TRUE) | — |
| Closures/captures | Deferred | ✅ Done (VM opcodes + upvalue format + GC tracing; compiler capture analysis partial) | — |
| Exceptions | Deferred | ✅ Done (try_begin/try_end/throw, handler stack, frame unwinding) | — |
| Classes/objects | Deferred | ❌ Not implemented | LATER |
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
| Records/structs | Required v0+ | ✅ Done (OBJ_RECORD, field access by name) | — |
| Mark-sweep GC | Required when heap exists | ✅ Done (mark roots → sweep, allocation threshold trigger) | — |
| GC stress mode | Required by standards | ✅ Done (collect-every-alloc mode for testing) | — |

### 3.3 Compiler Infrastructure (from compiler doc, CPython analysis)

| Feature | Source | Implementation Status | Priority |
|---------|--------|----------------------|----------|
| Arena allocator for compiler | CPython pattern, eng standards §19 | ✅ Done (`src/common/arena.h/.c`) | — |
| Separated scope resolution pass | Compiler doc §7 | ⚠️ Inline in semantic.c | MEDIUM |
| Type checking pass | Compiler doc §8 | ✅ Done (`src/typecheck/typecheck.c`) | — |
| Constant folding | Compiler doc §16.2 | ❌ Missing | LOW |
| Short-circuit boolean lowering | Compiler doc §14.4 | ✅ Done (compiler emits JUMP_IF_FALSE/TRUE for and/or) | — |
| Loop lowering (while, break, continue) | Compiler doc §14.2-14.3 | ✅ Done (while loops compile and run) | — |
| Closure/capture analysis | Compiler doc §15.3 | ⚠️ VM + format wired; compiler capture analysis basic | MEDIUM |
| Error recovery (continue past errors) | Eng standards §6, CPython pattern | ✅ Done (HIR_POISON propagation, multi-diagnostic) | — |
| Opcode generation from data file | Eng standards §8, CPython pattern | ❌ Hand-maintained (X-macro is single source of truth) | LOW |

### 3.4 Testing & Quality (from eng standards §25-28, CPython analysis)

| Feature | Source | Implementation Status | Priority |
|---------|--------|----------------------|----------|
| Per-subsystem test files | Eng standards §25 | ✅ Done (10 test files: bytecode, vm, vm_collections, gc, graph, pipeline, pipeline_e2e, infra, runtime, error_recovery) | — |
| LOC enforcement | Eng standards | ✅ Done (600/file, 60/function, CTest `check_limits`) | — |
| Golden tests (disassembly, IR dumps) | Eng standards §25.5 | ⚠️ 1 golden disassembly test | MEDIUM |
| Compile-fail tests | Eng standards §25.3 | ✅ Done (test_compile_fail_diagnostic) | — |
| Runtime-fail tests | Eng standards §25.4 | ✅ Done (division_by_zero, stack_overflow, unhandled_throw) | — |
| Property-based tests | Eng standards §27 | ✅ Done (roundtrip_random) | — |
| Conformance test suite | Eng standards §25.7 | ❌ Missing | LATER |
| Differential testing (interpreter vs VM) | Vision doc §17 | ✅ Done (ref_interp_5_plus_10, ref_interp_function, ref_interp_not) | — |
| 500+ tests before public release | CPython analysis recommendation | ❌ At 123 | HIGH |

### 3.5 Documentation (from eng standards §57-58)

| Feature | Source | Status | Priority |
|---------|--------|--------|----------|
| Architecture overview | Eng standards | ✅ `docs/internals/architecture.md` | — |
| Bytecode format spec | Vision doc §27 | ⚠️ `docs/reference/bytecode-format.md` (stale — missing new opcodes) | MEDIUM |
| Opcode reference | Vision doc §27 | ⚠️ `docs/reference/opcode-reference.md` (stale — only 23 of 41 opcodes) | MEDIUM |
| Semantic graph spec | Vision doc §27 | ⚠️ `docs/reference/semantic-graph.md` (stale — missing new node kinds) | MEDIUM |
| Testing strategy doc | Eng standards | ⚠️ `docs/internals/testing.md` (stale — describes old single-file structure) | MEDIUM |
| Engineering standards | — | ✅ `docs/ARCANA_REPOSITORY_ENGINEERING_STANDARDS.md` | — |
| VM guardrails | Design constraint | ✅ `docs/design/vm-guardrails.md` | — |
| Language specification | Eng standards §24 | ❌ Missing | MEDIUM |
| HIR/MIR format docs | Vision doc §27 | ❌ Missing | LOW |
| Contributor guide | Eng standards §58 | ❌ Missing | LOW |

### 3.6 Guardrail Compliance (from `vm-guardrails.md`)

The VM guardrails document defines the CPython counterfactual test and topology-derived semantics requirements. Current compliance:

| Guardrail | Status | Notes |
|-----------|--------|-------|
| §4 Lower-Away Rule: compile-time resolution | ✅ Applied | Clockwise order → operand indices; containment → scope; layout → erased |
| §5 No weird opcodes for justification | ✅ Compliant | All 41 opcodes serve conventional semantics; none exist for aesthetic reasons |
| §6.1 Dependency-aware execution | ❌ Not started | Edges represent data flow but no FORK/JOIN/scheduling primitives |
| §6.2 Runtime regions | ❌ Not started | Regions are compile-time only; no ENTER_REGION/LEAVE_REGION |
| §6.3 Reactive/dataflow execution | ❌ Not started | No ACTIVATE/BIND/PROPAGATE |
| §6.4 Effect/capability boundaries | ❌ Not started | No ENTER_CAPABILITY/LEAVE_CAPABILITY |
| §7 Bytecode + metadata strategy | ⚠️ Partial | Debug section has source map; no region table or dependency table |
| §10 Anti-patterns avoided | ✅ | Not a Python frontend; not preserving geometry in bytecode unnecessarily |
| §11 Review questions answered | ❌ Not yet applied | Should be applied to each new feature PR |
| §12 Stage 1 milestone | ✅ Complete | Custom bytecode + VM execute conventional semantics correctly |
| §12 Stage 2 milestone | ❌ Not started | No topology-derived semantic constructs yet |

---

## 4. Development Roadmap

### Phase 1: Language Completeness ✅ COMPLETE

All v0 language features working: float, strings, loops, functions, branches, arrays, maps, exceptions, closures, bitwise, type casts, short-circuit booleans, 9 intrinsics.

### Phase 2: Compiler Hardening ✅ MOSTLY COMPLETE

- ✅ Arena allocator (`src/common/arena.h/.c`)
- ✅ Type checking pass (`src/typecheck/typecheck.c`)
- ✅ Error recovery (HIR_POISON propagation, multi-diagnostic)
- ✅ Short-circuit boolean lowering
- ⚠️ Separated scope resolution (inline in semantic.c, functional but not separated)
- ❌ Constant folding (low priority)

### Phase 3: Test Infrastructure ✅ COMPLETE

- ✅ Split test suite (10 per-module test files + test_all.c driver with sub-runners)
- ✅ LOC enforcement (600/file, 60/function via `tools/check-limits.py` + CTest)
- ✅ Error recovery tests (5 tests: multi-error, poison propagation, dump, validation)
- ✅ Compile-fail tests
- ✅ Golden disassembly test
- ✅ Differential tests (interpreter vs VM)
- ✅ Property-based roundtrip test

### Phase 4: Topology-Derived Semantics (NEXT — Stage 2 per guardrails)

This is the critical phase where Arcana must begin earning its custom VM. Per `docs/design/vm-guardrails.md` §6, explore runtime concepts that emerge naturally from the spatial/topological programming model.

**Apply the Lower-Away Rule (§4) to each**: can its meaning be fully determined at compile time?

#### 4.1 Dependency-Aware Execution (guardrails §6.1)
- Source edges express real data dependencies
- Independent branches are known explicitly from graph topology
- Potential: FORK/JOIN opcodes, task scheduling, parallel regions
- **Design document required first** — answer §11 review questions

#### 4.2 Runtime Regions (guardrails §6.2)
- Region kinds that create runtime behavior: parallel domains, effect scopes, transactional scopes, arena/lifetime scopes
- Evaluate which survive the lower-away rule vs which resolve at compile time
- **Design document required first**

#### 4.3 Directed Cycles as Iteration
- A cycle in the semantic graph = loop structure
- Alternative to explicit `while` node — drawing-native iteration
- Compiler detects cycles and lowers to loop MIR
- This is topology-derived: the loop IS the drawing, not a `while` keyword

#### 4.4 Concentric Regions as Effect Domains
- Nested typed regions = transaction/effect/lifetime scoping
- Foundation for effect system
- Natural fit for capability boundaries (guardrails §6.4)

#### 4.5 Bytecode Metadata (guardrails §7.1)
- Region table in .mgc format (region kind, parent, properties)
- Dependency table (node→node data dependencies)
- Useful for runtime scheduling, debugging, profiling, visualization
- Preserves information without adding exotic opcodes

### Phase 5: Ecosystem Foundation

#### 5.1 Module System
- File-level compilation units
- Symbol export/import
- Semantic graph references to external circles
- Bytecode linking

#### 5.2 WASM Target
- Emscripten build of arcana-run
- Browser-based semantic graph visualization
- Natural fit: drawing tool → browser → WASM VM

#### 5.3 Language Specification
- Formal specification document in `spec/`
- Covers all implemented semantics
- Conformance test suite derived from spec

---

## 5. Opcode Generation from Data (Engineering Standards §8)

The engineering standards require a single source of truth for opcodes. Current state: X-macro in `opcodes.h` IS the single source of truth — all consumers (VM, verifier, assembler, disassembler) derive from it.

This is functional but not ideal. A data file (TOML/CSV) would be more readable and could generate documentation automatically.

**Low priority** — the X-macro approach works and is proven.

---

## 6. Implementation Order (Prioritized)

```
 ✅  Float support (opcodes, VM, compiler, tests)
 ✅  While loops (graph→HIR→MIR→bytecode, tests)
 ✅  String object type + GC (runtime/, heap objects, mark-sweep)
 ✅  Arrays + maps (runtime collections, VM opcodes)
 ✅  Exception handling (try/catch/throw, handler stack)
 ✅  Bitwise + type casts (6 bitwise ops, 3 cast ops)
 ✅  Extended intrinsics (type, assert, tostring, len, push, keys)
 ✅  Compiler completeness (all 41 opcodes emitted)
 ✅  Short-circuit booleans (and/or lowering)
 ✅  Closure VM wiring (upvalue format, serialization, GC tracing)
 ✅  Arena allocator (common/arena.h/.c)
 ✅  Type checking pass (typecheck/typecheck.c)
 ✅  Error recovery (HIR_POISON, multi-diagnostic)
 ✅  Split test suite (10 per-module files)
 ✅  LOC enforcement (600/file, 60/function)
 ✅  GC stress mode
 ✅  Records/structs (OBJ_RECORD)
--- above completed ---
1.  Update reference docs (opcode-reference.md, semantic-graph.md, bytecode-format.md)
2.  Separated scope resolution pass (extract from semantic.c)
3.  Directed-cycle-as-loop (geometry-native iteration — Stage 2 step)
4.  Bytecode metadata: region table + dependency table (guardrails §7.1)
5.  Dependency-aware execution design document (guardrails §6.1)
6.  Edge crossing semantics (design document first)
7.  Module system
8.  Constant folding
9.  More tests (target 500+)
10. Language specification
11. WASM/Emscripten build
```

---

## 7. Quality Gates

Before any public release or external visibility:

- [x] All v0 language features working (float, strings, loops, functions, branches, arrays, maps, exceptions, closures, bitwise, type casts, short-circuit booleans)
- [x] GC with stress mode passing all tests
- [ ] 500+ tests across split test files (at 123)
- [x] Golden tests for disassembly
- [x] Compile-fail test suite
- [x] Differential testing (interpreter vs VM) automated
- [x] Zero sanitizer warnings (ASan, UBSan, LeakSan)
- [x] Zero compiler warnings on GCC, Clang, MSVC (CI green on all 4 jobs)
- [x] LOC enforcement automated (600/file, 60/function, CTest)
- [ ] Architecture docs current (opcode/semantic-graph refs stale)
- [ ] Engineering standards compliance self-audit
- [ ] At least one geometry-native semantic test (operand meaning from topology, not text)
- [ ] CPython counterfactual review completed (guardrails §9)
- [ ] At least one topology-derived runtime concept (guardrails Stage 2)

---

## 8. Principles (Carried from Vision Documents + Guardrails)

1. **The drawing is the language.** Never accidentally create a text syntax that becomes the "real" language.
2. **Correctness before cleverness.** No optimization until unoptimized behavior is tested.
3. **Determinism.** Same input → same bytecode, same execution.
4. **Each layer owns its semantics.** Lower layers never reach upward.
5. **Structured diagnostics.** Every error has a stable code and source provenance.
6. **The VM is boring where boring suffices.** Conventional opcodes for conventional semantics (guardrails §7).
7. **The VM must earn its existence.** Arcana-specific runtime primitives only when topology-derived semantics require them (guardrails §3).
8. **Build from the bottom up.** Don't wait for the editor; prove the design with the executable stack.
9. **Smallest reversible design.** When uncertain, implement the minimal version that preserves architecture.
10. **Lower-away rule.** If a geometric concept can be fully resolved at compile time, erase it. If not, give it a runtime primitive (guardrails §4).

---

## 9. Session Protocol

Every development session:

1. Read this document's §3 gaps table — pick the top unfinished item
2. Build and test current state: `cmake -B build && cmake --build build && ctest --test-dir build -C Debug -V`
3. Implement the feature with tests
4. For new runtime/VM features: answer the §11 review questions from `docs/design/vm-guardrails.md`
5. Update the gaps table in this document (move ❌ → ✅)
6. Commit with descriptive message
7. Push to feature branch, create PR if CI passes
