# Native Semantics & VM Design Guardrails

**Status:** Architecture guardrail / design intent
**Source:** `arcana_native_semantics_vm_guardrails.docx`

This document is a standing design constraint for compiler/runtime work. It exists so future implementation work does not accidentally optimize away the reason Arcana owns its compiler, bytecode, runtime, and VM.

---

## 1. Executive Principle

Arcana does not need a custom VM merely because building one is interesting. The VM must eventually be justified by language semantics that are awkward, lossy, or inefficient to express as a thin translation into CPython.

**Weak long-term justification:**
Arcana drawing → conventional AST → ordinary imperative semantics → custom stack VM

**Strong long-term justification:**
Arcana drawing/topology → Semantic Graph → Arcana-specific semantic lowering → runtime concepts that preserve useful graph/region/effect meaning → Arcana bytecode + VM

---

## 2. The CPython Counterfactual

A competent engineer can always propose: Arcana drawing → Python AST → CPython compiler → Python bytecode → CPython VM. This would provide mature GC, exceptions, objects, modules, a debugger ecosystem, a standard library, portability, and years of runtime engineering.

Arcana should beat this alternative on **design grounds**, not dismiss it emotionally.

### 2.1 When CPython would be enough

If rings are merely graphical braces. If lines are merely a prettier way to spell assignment. If cycles are translated directly into conventional loops without retaining useful semantics. If all runtime values are effectively Python objects. If Arcana bytecode is simply a differently encoded form of ordinary stack-machine operations.

---

## 3. What the Custom VM Must Earn

The VM earns its existence when Arcana programs have important runtime semantics that would require a substantial Arcana runtime framework on top of Python rather than a straightforward compilation to Python constructs.

The goal is not to preserve geometry for its own sake. The goal is to preserve runtime information when that information is **useful** to Arcana semantics, execution, optimization, safety, or tooling.

---

## 4. The Lower-Away Rule

Every topology-aware feature should face this question: **can its meaning be fully determined at compile time?**

**If YES:** lower the geometry away. Preserve only the resulting semantics + source provenance.

**If NO:** represent the surviving concept explicitly in MIR / bytecode / runtime metadata and give the VM/runtime a real semantic primitive for it.

### 4.1 Should usually disappear

- Clockwise ordering → argument indices
- A lexical enclosure → resolved scope and capture set
- A simple topological loop → conventional control-flow blocks and backward jumps
- A visual constant rune → constant-pool entry
- Purely aesthetic geometry and layout coordinates

### 4.2 May legitimately survive

- Parallel execution domains
- Reactive dependencies and activation rules
- Effect/capability boundaries enforced at runtime
- Transactional regions
- Structured lifetime/arena regions if they influence runtime allocation
- Graph synchronization, joins, or barriers
- Persistent dependency graphs used by incremental/reactive execution

---

## 5. Do Not Make the VM Weird Just to Justify It

**Correct direction:** useful Arcana language semantics → identify what survives lowering → design runtime abstraction → design bytecode primitive if warranted

**Incorrect direction:** "we built a custom VM" → invent arbitrary custom opcodes → force source semantics to use them

---

## 6. Arcana-Native Runtime Concepts Worth Exploring

These are research directions, not mandatory features. They are valuable because they can emerge naturally from a spatial/topological programming model.

### 6.1 Dependency-aware execution

If source edges express real data dependencies, independent branches may be known explicitly rather than rediscovered by an optimizer. Potential: FORK/JOIN for task scheduling, parallel execution, GPU dispatch.

### 6.2 Runtime regions

Region kinds that may create runtime behavior: parallel execution domains, effect/capability scopes, transactional scopes, arena/lifetime scopes, exception or cancellation domains, reactive update domains.

### 6.3 Reactive/dataflow execution

Programs where nodes become runnable because dependencies are satisfied or values change. Graph topology is execution, not merely source organization.

### 6.4 Effect and capability boundaries

A topological boundary may grant or constrain I/O, mutation, networking, native calls, or resource access. Runtime enforcement may remain useful for safety, sandboxing, or dynamic capabilities.

---

## 7. Bytecode Strategy

Arcana bytecode should **stay boring wherever boring bytecode is sufficient**. Conventional opcodes (CONST, LOAD_LOCAL, ADD, CALL, RETURN, JUMP) are appropriate for conventional semantics. Arcana-native operations (ENTER_REGION, FORK, JOIN, ACTIVATE, BIND, ENTER_CAPABILITY) should be introduced only when needed.

### 7.1 Bytecode + metadata is often better than exotic instructions

Many Arcana-specific facts can live in structured executable metadata rather than the instruction stream:

```
ARCANA EXECUTABLE
  CODE:  LOAD_LOCAL 0 / CALL 3 / RETURN
  REGION TABLE:    R0 lexical, R1 parallel, R2 io-capability
  DEPENDENCY TABLE:  N1 → N4, N1 → N5
  SOURCE MAP:  instruction 18 ← glyph G42
```

This preserves information for runtime scheduling, debugging, profiling, and visualization without turning the opcode set into a mirror of the editor.

---

## 8. The VM Should Eventually Enable Things Python Makes Awkward

- A value model chosen for Arcana rather than inherited Python object semantics
- Predictable primitive numeric types
- A concurrency model that exploits explicitly represented dependency topology
- A runtime ABI designed for embedding without shipping CPython
- A verifier and sandbox model designed around Arcana bytecode
- Arcana-specific debugging (execution mapped back onto graph/ring/glyph source elements)
- Potential WebAssembly, native/JIT, GPU, or alternate VM backends from the same lower IR

---

## 9. The Python Backend Test

A useful future experiment: an unofficial Arcana-to-Python backend. When generated Python increasingly resembles an interpreter/runtime API for the Arcana abstract machine (`_arc_enter_region(...)`, `_arc_fork_domain(...)`, `_arc_propagate(...)`), the custom VM has justified itself empirically.

---

## 10. Anti-Patterns the Agent Must Avoid

1. Do not turn Arcana into Python/Java semantics with decorative geometric syntax.
2. Do not introduce a pleasant textual high-level Arcana language as the primary implementation path.
3. Do not preserve geometry in bytecode when its meaning can be cleanly resolved at compile time.
4. Do not erase graph/region information prematurely if it could matter to scheduling, effects, lifetimes, or runtime semantics.
5. Do not invent custom opcodes solely because they sound Arcana-specific.
6. Do not let the VM inspect source-level graph patterns that should have been lowered by the compiler.
7. Do not inherit Python semantics accidentally.
8. Do not optimize for CPython interoperability at the expense of Arcana semantic independence.
9. Do not treat the current stack VM as sacred; it is an implementation technique.
10. Do not let early bootstrap shortcuts become undocumented permanent semantics.

---

## 11. Architectural Review Questions

For every major language/runtime feature, answer:

1. What semantic concept does this feature introduce?
2. Is that concept inherent to Arcana topology/drawing, or merely conventional syntax represented visually?
3. Can the concept be completely resolved during compilation?
4. If yes, at which lowering stage should it disappear?
5. If no, what exact runtime semantic survives?
6. Does the VM already have an appropriate primitive, or is a new primitive justified?
7. Could the same behavior be implemented cleanly by compiling to Python?
8. If Python could implement it, what would be lost: performance, safety, semantics, portability, embedding, determinism, or nothing?
9. Would the proposed runtime feature remain coherent if a second Arcana frontend existed?
10. Would the feature remain coherent if Arcana gained a native/JIT/WebAssembly backend later?

---

## 12. Milestones for Proving VM Independence

| Stage | Evidence |
|-------|----------|
| Stage 0 — Ownership | Custom bytecode and VM execute conventional semantics correctly. Benefit: implementation control and experimentation surface. |
| Stage 1 — Semantic independence | Arcana has its own type/value/error/function semantics rather than mirroring Python behavior. |
| Stage 2 — Topology-derived semantics | Important language constructs are inferred from regions, dependencies, cycles, boundaries, or ordering rather than represented only as ordinary AST node labels. |
| Stage 3 — Runtime-native graph concepts | At least one useful topology-derived concept survives lowering and has first-class runtime treatment (dependency scheduling, effect regions, reactive activation, structured parallelism). |
| Stage 4 — Backend leverage | The compiler exploits Arcana information for optimizations or targets awkward through CPython (task graphs, SIMD/GPU, sandboxing, native embedding, JIT). |
| Stage 5 — Counterfactual proof | A Python backend remains possible but requires a meaningful Arcana runtime/emulation layer and is clearly not the canonical execution model. |

---

## 13. Recommended Near-Term Priority

1. Keep the stack VM and bytecode implementation clean and correct.
2. Complete the Semantic Graph → HIR → MIR → bytecode boundaries.
3. Move more source semantics from explicit conventional node kinds toward genuine topology-derived meaning where that improves the language.
4. Preserve potentially useful dependency/region information into HIR/MIR instead of erasing it too early.
5. Add runtime primitives only when a real feature requires them.
6. Maintain source provenance so VM execution can eventually illuminate the original circle during debugging/profiling.
7. Periodically perform the CPython counterfactual review.

---

## 14. Definition of Success

Arcana succeeds architecturally when all three statements are true:

1. The drawing/topology is the canonical source-language structure, not a cosmetic encoding of a hidden textual language.
2. Most geometric syntax is compiled away cleanly into normal compiler concepts when runtime geometry is unnecessary.
3. The remaining Arcana-specific runtime semantics are important enough that compiling through CPython would require emulating the Arcana abstract machine rather than simply translating syntax.
