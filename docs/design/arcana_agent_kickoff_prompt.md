# Arcana Agent Kickoff and Implementation Prompt

## Purpose

This document contains two prompts for an autonomous coding agent that will begin turning Arcana into a real programming language implementation.

The agent must treat these two existing design documents as required inputs:

- `arcana_compiler_architecture.docx`
- `arcana_vm_compiler_runtime_architecture.docx`

The documents establish the conventional compiler and VM foundation. The newer design direction in this prompt adds one important refinement: Arcana's canonical source semantics are not a conventional textual AST. They are a normalized, drawing-derived semantic topology. The image/stroke recognition layer remains out of scope, but topology such as containment, directed connections, cyclic port order, and region boundaries is allowed to be language syntax and must survive long enough in the compiler to determine program meaning.

Use the **Basic Prompt** when you want the agent to start quickly. Use the **Master Prompt** when starting a new repository, a long-running coding session, or a multi-agent effort where architectural drift would be costly.

---

# Basic Prompt

You are implementing Arcana, a programming language whose canonical source syntax is a drawn magic-circle program rather than ordinary text. Begin by reading `arcana_compiler_architecture.docx` and `arcana_vm_compiler_runtime_architecture.docx` in full. Treat them as the baseline architecture, but apply the newer requirement that the drawing must be semantically native: below the image/editor layer, the compiler receives an **Arcana Semantic Graph** containing nodes, directed edges, regions, containment, and discrete embedding/topology information such as clockwise port order. Do not reduce the drawing immediately to a conventional textual AST and do not build image recognition or UI yet.

Use CPython as an engineering reference for how a real high-level language is built from source through semantic analysis, compiler IR, bytecode/code objects, and a virtual-machine interpreter, but do not copy Python's language semantics or its implementation complexity. Arcana should have its own versioned bytecode format, assembler/disassembler, verifier, compiler, runtime value model, call frames, and VM. The lower compiler may become conventional after the Arcana Semantic Graph is lowered into HIR/MIR.

Start doing implementation work, not just planning. First inspect the repository and existing code. If no implementation language has been selected and the repository is empty, default to Rust unless repository constraints strongly suggest otherwise; record the choice in a short architecture decision. Build the smallest testable vertical slice in stages: bytecode definitions -> assembler/disassembler -> verifier -> stack VM -> semantic graph model -> topology-aware lowering -> HIR/MIR -> bytecode emission -> execution. Hand-author semantic graph fixtures; do not wait for the drawing editor.

For the first geometry-native subset, support at least: **region containment as lexical scope**, **directed edges as value/data dependencies**, and **clockwise/cyclic port order as operand or argument order**. Keep more exotic semantics such as crossings, winding, symmetry-driven parallelism, and closure capture behind explicit future extensions unless needed for the first vertical slice.

The first end-to-end target should be a hand-authored semantic graph that represents `5 + 10`, compiles to Arcana bytecode, passes the independent verifier, executes on the Arcana VM, and produces `15`. Then add function calls, branching, and recursion until `fib(10)` produces `55`. Preserve stable semantic element IDs and bytecode source maps so a future editor can highlight the originating rune/ring during errors and debugging.

Keep documentation and tests current as you work. When a design choice is uncertain, prefer the smallest reversible design that preserves the architecture. Do not silently invent a normal Python-like user-facing syntax; textual representations are permitted only as assembly, debug dumps, tests, or a serialization of Arcana's semantic graph. At the end of each work cycle, report what you implemented, tests run, architectural decisions made, known gaps, and the next concrete milestone.

---

# Master Prompt

## 1. Role and Mission

You are the implementation agent for **Arcana**, a new high-level programming language with its own compiler, bytecode format, virtual machine, runtime, verifier, and development tooling.

Arcana's defining property is that its canonical source language is **spatial and drawing-based**. A user will eventually construct programs by drawing magic circles, rings, runes, connections, and nested structures. The drawing is not merely a skin over Python, Java, or another normal textual language. Discrete properties of the drawing are intended to be part of the language grammar and semantics.

Your mission is to turn the existing architecture into a functioning language implementation from the bottom up, while preserving a clean boundary below the image/rendering/editor layer.

You are expected to write code, tests, specifications, and engineering notes. Do not stop after producing another architecture proposal unless a blocking design decision genuinely prevents implementation.

## 2. Required Inputs

Before modifying or creating implementation code, read these documents completely:

1. `arcana_compiler_architecture.docx`
2. `arcana_vm_compiler_runtime_architecture.docx`

If your environment cannot directly read DOCX files, extract their text with a reliable local method such as `python-docx` or by reading the DOCX XML. Do not skip the documents because of file format friction.

Treat them as the baseline for:

- compiler layering;
- HIR and MIR;
- semantic analysis;
- bytecode emission;
- stack VM design;
- call frames;
- value/object model;
- bytecode file layout;
- loader and verifier;
- assembler/disassembler;
- runtime intrinsics;
- debugging/source maps;
- garbage collection roadmap;
- testing strategy.

### 2.1 Newer requirement that refines the documents

Where the documents assume a generic "Semantic Program Model" or an AST-like compiler input, refine that boundary into a geometry-native **Arcana Semantic Graph**.

The current architectural boundary is:

```text
DRAWING / EDITOR / STROKES / RENDERING          out of scope for now
                    |
                    v
        Geometric Scene / Editor Objects         future frontend
                    |
                    v
          Topological Normalization              future frontend boundary
                    |
                    v
        ARCANA SEMANTIC GRAPH                     canonical compiler input
        - nodes
        - directed edges
        - regions
        - containment
        - cyclic port order
        - boundary relationships
        - selected crossings/topology
        - stable source element IDs
                    |
                    v
        Semantic analysis / typed HIR
                    |
                    v
               CFG / MIR
                    |
                    v
          Arcana bytecode (.mgc)
                    |
                    v
             Arcana VM/runtime
```

The compiler must never need pixels, brush strokes, anti-aliasing, exact screen coordinates, or computer vision. However, it **may and should receive normalized topological facts derived from the drawing** when those facts are part of the language.

This distinction is critical:

- "No pixels in the compiler" remains a hard rule.
- "No geometry-derived semantics in the compiler" is **not** a rule.
- Arcana's drawing should be capable of expressing semantics that would be awkward to recreate as an ordinary Python-like textual language.

## 3. Architectural Intent

The target system should eventually look like this:

```text
Magic-circle editor
        |
        v
normalized topology
        |
        v
Arcana Semantic Graph
        |
        +--> structural/topological validation
        +--> symbols/scopes/types/effects
        |
        v
HIR
        |
        v
MIR / CFG
        |
        v
symbolic Arcana bytecode
        |
        v
encoded .mgc file
        |
        +--> independent verifier
        |
        v
Arcana VM
        |
        v
runtime result / diagnostics
```

The implementation should preserve a deliberate semantic destruction boundary: the drawing/topology determines meaning first; after that meaning is made explicit in HIR/MIR, many geometric details can disappear from executable bytecode. Debug/source metadata should retain enough identity to map execution and errors back to the source drawing later.

## 4. What Makes Arcana Drawing-Native

Do not implement Arcana as "Java with runes" or "Python whose tokens happen to be pictures."

The canonical source model should use **discrete, robust spatial/topological relationships** as grammar. Favor relations that can be snapped or normalized exactly by an editor rather than fragile pixel measurements.

### 4.1 Version-0 geometry-native semantics

Implement or design the first vertical slice around these three semantics:

1. **Region containment establishes lexical scope or semantic ownership.**
   - A node inside a region belongs to that region.
   - Nested regions create nested scopes/semantic contexts.
   - Moving a node without crossing a semantic boundary should not change meaning.

2. **Directed connections establish data/value dependencies.**
   - An edge means a semantic relationship, not merely a decorative line.
   - Edge endpoints must reference stable node/port identities.
   - Shared values may feed multiple consumers.

3. **Cyclic/clockwise port order establishes operand or argument order.**
   - The order of edges around a node can matter even when the connectivity graph is otherwise identical.
   - The compiler should receive normalized cyclic order, not infer it from floating-point coordinates.

These are enough to prove that Arcana is not merely a conventional AST rendered as a circle.

### 4.2 Geometry-native semantics to preserve as future extensions

Design the semantic graph so these can be added without replacing the core data model, but **do not implement all of them immediately**:

- edge crossing as interaction, synchronization, composition, or binding;
- crossing a region boundary as capture/export/capability transfer;
- directed cycles as explicit iteration/control-flow structure;
- concentric or typed regions as effect/lifetime/transaction domains;
- rotational or mirror symmetry as parallel/fan-out semantics;
- repeated radial motifs as map/vectorized constructs;
- winding/topological relations around marked nodes;
- runtime regions or graph-native execution where compile-time lowering is insufficient.

Prefer topology over precise geometry. Avoid language rules based on exact angles, pixel distances, circle radii, or tiny placement differences.

## 5. Arcana Semantic Graph: Initial Data Model

Create an explicit module/crate/package for the semantic graph. The exact host-language representation is your decision, but it should conceptually support structures similar to:

```text
ProgramGraph
  modules: [ModuleGraph]
  elements: StableId -> Element
  provenance: StableId -> SourceRef

ModuleGraph
  root_region: RegionId
  nodes: [NodeId]
  edges: [EdgeId]
  regions: [RegionId]

Node
  id: NodeId
  kind: NodeKind
  region: RegionId
  ports: [PortId]
  cyclic_port_order: [PortId]
  attributes: ...

Port
  id: PortId
  owner: NodeId
  direction: Input | Output | Bidirectional
  role: Optional<SemanticRole>

Edge
  id: EdgeId
  from: PortId
  to: PortId
  boundary_crossings: [RegionBoundaryId]
  attributes: ...

Region
  id: RegionId
  parent: Optional<RegionId>
  kind: RegionKind
  members: [StableId]
  boundary_ports: [PortId]

SourceRef
  stable_element_id: StableId
  optional_subregion: ...
```

This is a conceptual contract, not a demand to copy these fields verbatim.

### 5.1 Invariants

At minimum, make validation capable of enforcing:

- all IDs are unique and resolvable;
- every node belongs to exactly one valid region unless explicitly global/root;
- region parentage is acyclic;
- every edge endpoint references a valid port;
- a port's owner exists;
- cyclic port order contains each relevant port exactly once;
- input cardinality rules are satisfied for known node kinds;
- graph fragments required by the selected semantic subset are connected/complete enough to compile;
- malformed or editor-incomplete graphs produce structured diagnostics rather than panics;
- source/provenance IDs survive lowering.

## 6. Use CPython as an Engineering Reference, Not a Template

Study CPython conceptually as a mature example of how a high-level language implementation separates concerns.

Use the following analogy:

| CPython concept | Arcana analogue |
| --- | --- |
| textual source | normalized drawing topology / Arcana Semantic Graph |
| parser output / AST | Semantic Graph plus typed HIR |
| symbol-table analysis | Arcana symbol/scope resolution |
| compiler control-flow representation | Arcana MIR / CFG |
| Python bytecode | Arcana bytecode |
| code object with constants/names/metadata | Arcana function record + constant pool + debug map |
| bytecode evaluation loop | Arcana VM dispatch loop |
| `dis` tooling | `arcana-dis` |
| bytecode cache/container | versioned `.mgc` executable container |
| runtime objects | Arcana `Value` / heap-object model |

The lessons to borrow are:

- separate source representation from executable instructions;
- perform semantic analysis before execution;
- lower high-level semantics into a smaller instruction vocabulary;
- use explicit function/code metadata and constant pools;
- maintain a VM evaluation loop with well-defined frames and stack behavior;
- make bytecode inspectable;
- test compiler and runtime layers independently;
- preserve source mappings for tracebacks/debugging.

Do **not** copy these aspects simply because Python has them:

- Python's dynamic semantics unless Arcana explicitly chooses them;
- CPython's historical implementation complexity;
- version-specific adaptive/JIT optimizations;
- Python's syntax or object model;
- Python's exact opcode set;
- Python's import system;
- the assumption that text is the canonical source form.

CPython is a reference for disciplined implementation layering. Arcana must remain its own language.

## 7. Textual Representations: Strict Boundary

Text is useful for engineering, but do not accidentally create a competing ordinary high-level Arcana syntax.

Allowed textual forms:

- Arcana bytecode assembly;
- disassembly;
- HIR/MIR debug dumps;
- JSON/YAML/compact fixture serialization of the Arcana Semantic Graph;
- test fixtures;
- diagnostics;
- generated inspection formats.

A semantic-graph fixture might look like:

```text
region r0 scope
node n0 const_int(5) in r0
node n1 const_int(10) in r0
node n2 add in r0 ports=[lhs,rhs,out] cyclic=[lhs,rhs,out]
edge e0 n0.out -> n2.lhs
edge e1 n1.out -> n2.rhs
root n2.out
```

This is acceptable because it **describes the drawing-native semantic structure**. It is not intended to become a pleasant alternative syntax for end users.

Do not implement a Python-like parser simply to make compiler development easier. Hand-authored graph fixtures already solve that problem without undermining the language design.

## 8. Host Implementation Language

First inspect the repository.

- If an implementation language and build system already exist, preserve them unless there is a compelling documented reason not to.
- If the repository is empty and no language is specified, default to **Rust** for the initial implementation because it is a good fit for a memory-sensitive VM/compiler codebase and provides strong enums, pattern matching, deterministic tooling, and safe ownership for early runtime work.
- Record the decision in an architecture decision record (ADR) or equivalent short design note.
- Do not spend an entire session debating languages. The decision must be reversible at the architectural boundaries defined by the documents.

If the repository owner later selects C++, Zig, Go, or another systems language, preserve the architecture rather than treating host-language choice as part of Arcana semantics.

## 9. Repository and Documentation Baseline

After reading the source documents, inspect the repository and create only the missing structure needed for the next executable milestone.

A reasonable logical decomposition is:

```text
arcana/
  docs/
    decisions/
    language/
    bytecode/

  semantic_graph/
  compiler/
    semantic/
    hir/
    mir/
    backend/

  bytecode/
    opcode/
    format/
    verifier/
    assembler/
    disassembler/

  vm/
    interpreter/
    value/
    frame/
    heap/
    intrinsic/

  tools/
    arcana-run/
    arcana-asm/
    arcana-dis/
    arcana-verify/
    arcana-compile/

  tests/
    semantic_graph/
    compiler/
    bytecode/
    vm/
    integration/
```

Do not create empty directories solely to match this diagram. Create structure as implementation requires it.

### 9.1 Required decision note

Create a short document such as:

`docs/decisions/0001-geometry-native-source-model.md`

It should state that:

- the two DOCX documents remain baseline architecture;
- the canonical compiler input is now the Arcana Semantic Graph rather than a generic conventional AST;
- normalized topology is semantic input;
- pixels/rendering remain outside the compiler;
- HIR/MIR and VM may remain conventional after topology has been lowered;
- textual graph fixtures and assembly are engineering interfaces, not canonical user source.

## 10. Implementation Strategy: Build From the Executable Core Outward

Do not wait for the magic-circle editor. The editor cannot validate the language if there is nothing executable beneath it.

Follow this order unless existing code makes another ordering more efficient.

### Milestone A - Bytecode vocabulary and in-memory instruction model

Implement a deliberately small instruction model supporting:

- constants;
- stack manipulation as required;
- integer arithmetic;
- comparison;
- local load/store;
- unconditional branch;
- conditional branch;
- function call;
- return;
- intrinsic call or print;
- halt.

Define every opcode's:

- operands;
- stack inputs;
- stack outputs;
- type expectations;
- runtime errors;
- encoding behavior.

Do not optimize opcode density yet.

**Definition of done:** unit tests can construct and decode a short instruction sequence deterministically.

### Milestone B - Assembler, disassembler, binary container, and verifier

Implement the engineering tools before the high-level compiler is complete.

Required capabilities:

- versioned `.mgc` header;
- constant pool;
- function records;
- code section;
- optional/debug source map section;
- binary reader/writer;
- textual assembler for bytecode;
- disassembler with byte offsets and operands;
- verifier that rejects malformed instructions, invalid jump targets, stack underflow, invalid constant/function indices, and inconsistent stack state at control-flow joins.

**Definition of done:** this conceptual assembly runs through assemble -> verify -> disassemble round trip:

```text
.const 5
.const 10
.func main 0
    const 0
    const 1
    add
    intrinsic print 1
    halt
.end
```

### Milestone C - Minimal stack VM

Implement:

- instruction pointer;
- operand stack;
- call-frame stack;
- local slots;
- function dispatch;
- runtime values needed by current instructions;
- intrinsic interface;
- structured runtime errors;
- optional trace mode.

Do not implement a JIT. Do not implement clever dispatch. Do not implement a sophisticated GC until heap objects require it.

**Definition of done:** the bytecode from Milestone B executes and prints `15`.

### Milestone D - Arcana Semantic Graph library and validator

Implement the semantic graph separately from HIR.

Support the first drawing-native semantic subset:

- regions and nesting;
- semantic nodes;
- typed/directed ports;
- directed edges;
- cyclic port order;
- stable source IDs;
- structured graph diagnostics.

Create hand-authored fixtures rather than a visual editor.

**Definition of done:** valid and invalid graph fixtures have deterministic parse/load and validation behavior.

### Milestone E - Topology-aware semantic lowering

Build the pass that interprets Arcana-specific topology and converts it into explicit compiler meaning.

For the initial subset:

- containment -> scope/ownership;
- directed edges -> value dependencies;
- cyclic port order -> operand/argument ordering.

The output should be typed or type-ready HIR. After this stage, normal compiler representations are allowed to look conventional.

**Definition of done:** a graph fixture representing `5 + 10` lowers to inspectable HIR equivalent to an integer addition with ordered operands.

### Milestone F - HIR, MIR/CFG, and bytecode backend

Implement enough of the architecture in `arcana_compiler_architecture.docx` to support the vertical slice:

- HIR node identities/provenance;
- symbol and scope resolution;
- primitive type checking or typed node classification;
- MIR temporaries/basic blocks;
- explicit evaluation order;
- symbolic bytecode;
- stackification;
- jump layout/patching;
- constant-pool/function-table emission;
- debug source-map emission.

**Definition of done:** semantic graph -> HIR -> MIR -> `.mgc` -> verifier -> VM prints `15`.

### Milestone G - Functions and control flow

Add, in order:

1. locals;
2. function definitions;
3. function calls;
4. return;
5. comparisons;
6. if/else;
7. loops or explicit cyclic control flow;
8. recursion.

Use hand-authored semantic-graph fixtures to represent these features.

**Definition of done:** `fib(10)` executes on Arcana's own VM and produces `55`.

## 11. First Vertical-Slice Semantics

Do not allow the first end-to-end program to enter through an ordinary textual AST.

The preferred acceptance test is conceptually this drawing-derived graph:

```text
Region r0

ConstInt(5)  ----\
                  Add ----> Root/Output
ConstInt(10) ----/
```

with an explicit normalized port order on `Add`.

The implementation should prove all of the following:

1. a semantic graph can be loaded or constructed;
2. its topology is validated;
3. operands are determined by topology/port order;
4. stable source IDs are retained;
5. it lowers to HIR;
6. HIR lowers to MIR;
7. MIR becomes Arcana bytecode;
8. bytecode is independently verified;
9. the VM executes it;
10. output is `15`;
11. disassembly can show the executed instructions;
12. debug metadata can map the `ADD` instruction back to the source semantic node.

This is the first proof that the drawing is part of Arcana's language design rather than merely presentation.

## 12. Language Semantics for Version 0

Keep the user-language surface small enough that every feature can be specified and tested.

Recommended initial value categories:

- `Null`;
- `Bool`;
- signed 64-bit `Int`;
- 64-bit `Float` after integer semantics are stable;
- immutable `String` when heap support is ready.

Recommended initial language features:

- literal values;
- arithmetic;
- comparisons;
- local bindings;
- lexical regions/scopes;
- functions;
- calls;
- return;
- if/else;
- iteration;
- basic intrinsic output.

Delay unless required:

- classes;
- inheritance;
- metaprogramming;
- macros;
- async/await;
- exceptions beyond structured runtime errors;
- closures/capture;
- FFI;
- package manager;
- JIT;
- native code generation;
- complex module resolution;
- generics;
- advanced optimization;
- graph-native runtime scheduling;
- symmetry/concurrency semantics.

## 13. Type-System Guidance

Do not blindly copy Python's dynamic typing. The existing architecture correctly notes that a statically known primitive subset makes the compiler and verifier much easier.

For v0, prefer one of these:

- a small static type system for primitive nodes; or
- explicit node kinds that make primitive operator types statically obvious.

For example, a general source `Add` may be resolved during semantic analysis to `IntAdd` or `FloatAdd`, even if the bytecode initially uses generic operations.

Type decisions must be documented. Avoid designing the entire eventual type system before the first executable program.

## 14. Bytecode Policy

Treat Arcana bytecode as an executable contract between compiler and VM, not as a serialization of HIR or the semantic graph.

The bytecode should be:

- deterministic;
- versioned;
- independently verifiable;
- easy to disassemble;
- compact enough for development but not prematurely optimized;
- decoupled from exact editor geometry;
- able to carry source/provenance metadata.

### 14.1 Geometry at bytecode level

Use this rule:

> If the meaning of a geometric/topological relationship is fully knowable at compile time, lower it away into ordinary executable operations while preserving debug/source metadata.

Examples:

- containment used only for lexical scope can disappear after symbol resolution;
- cyclic port order can become operand evaluation order;
- a drawing cycle used as a loop can become branches/back-edges;
- boundary crossing used as closure capture can become capture metadata/instructions.

If a geometric relation represents a genuinely runtime concept, then and only then introduce a runtime/bytecode primitive or auxiliary table for it.

Do not force the VM to understand rings merely because the source contains rings.

## 15. Debug and Provenance Requirements

Stable source identities are not optional.

Every user-authored semantic element should have a stable ID. Lowering passes should preserve origin/provenance mappings so the runtime can eventually support:

- stack traces naming semantic elements;
- highlighting the originating rune or connection;
- tracing which circle/region produced a value;
- profiling execution back onto a drawing;
- breakpoints keyed by semantic elements;
- editor diagnostics without relying on line/column positions.

At minimum, `.mgc` debug metadata should be able to map a bytecode range to a semantic element ID.

## 16. Testing Requirements

Tests are part of the language specification. Every milestone must add tests at the correct layer.

### 16.1 Bytecode tests

Test:

- encode/decode round trip;
- every opcode;
- invalid operands;
- malformed/truncated binaries;
- invalid jump targets;
- stack underflow;
- inconsistent stack heights;
- deterministic output bytes.

### 16.2 VM tests

Test:

- integer arithmetic;
- local load/store;
- branches;
- function calls;
- nested calls;
- recursion;
- runtime errors;
- call-stack limits;
- trace output where practical.

### 16.3 Semantic graph tests

Test:

- valid region nesting;
- invalid cyclic region ancestry;
- missing nodes/ports;
- invalid edges;
- duplicate IDs;
- cyclic port-order invariants;
- region containment semantics;
- operand order determined by cyclic order;
- stable IDs preserved through edits/serialization where relevant.

### 16.4 Compiler pass tests

Each pass should be testable independently using readable fixtures/golden dumps:

```text
Semantic Graph
      -> HIR golden
HIR   -> MIR golden
MIR   -> symbolic bytecode golden
bytecode -> disassembly golden
```

### 16.5 End-to-end tests

Maintain a growing suite:

1. constant output;
2. `5 + 10 == 15`;
3. arithmetic precedence expressed through graph structure;
4. local binding;
5. function `square(7) == 49`;
6. branch `max(5, 10) == 10`;
7. loop/counting test;
8. recursion `fib(10) == 55`;
9. malformed semantic graph produces a semantic diagnostic, not a VM crash;
10. malformed bytecode is rejected by verifier before execution.

## 17. Differential and Reference Testing

During early development, it is acceptable to keep a tiny direct interpreter for HIR or the Arcana Semantic Graph for pure features. Use it as an executable reference semantics:

```text
same semantic graph
    |                \
    v                 v
reference evaluator   compiler -> bytecode -> VM
    |                 |
    +---- compare ----+
```

This plays a role similar to having an easier-to-understand specification implementation. It can catch lowering/backend bugs without making the reference evaluator the production runtime.

Do not use Python execution as Arcana's reference semantics. Python is an architectural comparator, not Arcana's oracle.

## 18. Verifier and Trust Boundary

The VM must not blindly trust compiler output.

Keep the verifier independently callable and testable. It should reject malformed bytecode before normal execution.

The compiler should also validate its own output in debug/test builds, but verifier independence is important because future bytecode may come from caches, external tooling, or corrupted files.

Treat verifier failures on compiler-generated bytecode as compiler bugs.

## 19. Runtime and Memory Roadmap

Do not implement heap complexity before it is required.

Suggested order:

1. immediate primitive values only;
2. functions/call frames;
3. immutable strings;
4. arrays/records;
5. mark-sweep GC;
6. closures;
7. richer objects.

When GC begins, create a stress mode that collects extremely frequently to expose missing roots.

Keep runtime intrinsics small and explicit. `print` and a monotonic clock are sufficient early examples. A general FFI is out of scope for v0.

## 20. Optimization Policy

Correctness and inspectability outrank speed during initial implementation.

Do not implement:

- JIT compilation;
- register allocation for native CPUs;
- aggressive inlining;
- speculative specialization;
- complex constant propagation frameworks;
- concurrent GC;
- exotic tagged-pointer schemes;
- adaptive opcode rewriting.

Simple constant folding and dead-code cleanup are acceptable only after unoptimized behavior is tested.

Keep every IR printable. The unoptimized path must remain easy to understand.

## 21. Determinism

Identical semantic input and compiler options should produce identical:

- HIR dumps;
- MIR dumps;
- symbolic bytecode;
- constant/function ordering where specified;
- `.mgc` output bytes;
- diagnostics ordering where possible.

Avoid iteration-order dependence from hash maps when serialized output matters. Determinism will make debugging, golden tests, caching, and future visual diffs much easier.

## 22. Error Handling and Diagnostics

Never make the editor layer responsible for all validation.

The compiler must produce structured diagnostics for malformed or semantically invalid Arcana graphs.

A diagnostic should conceptually include:

```text
severity
error_code
message
primary SourceRef / StableId
related SourceRefs
optional semantic role/port
notes/help
```

Do not hardcode UI-specific prose or screen coordinates into the compiler.

The future editor should be able to transform a `SourceRef` into a highlighted rune, edge, ring, or port.

## 23. Engineering Rules

Follow these rules unless an existing repository convention clearly supersedes them:

1. Read before rewriting. Preserve useful existing code.
2. Keep compiler, bytecode, verifier, and VM separable.
3. One shared definition owns opcode numbers and binary format structures.
4. Prefer explicit enums/types over stringly typed internal APIs.
5. Make invalid internal states hard to construct after validation.
6. Keep parsing/serialization separate from semantic validation.
7. Avoid global mutable compiler state.
8. Make passes deterministic and individually testable.
9. Preserve stable semantic IDs through every lowering stage.
10. Keep textual debug dumps canonical enough for golden tests.
11. Do not introduce a normal high-level text syntax as a shortcut.
12. Do not build the visual editor until the lower vertical slice works.
13. Do not optimize before correctness is demonstrated.
14. Update docs when implementation intentionally diverges from the DOCX architecture.
15. Prefer a small working language over a broad half-implemented language.

## 24. How to Handle Ambiguity

You have autonomy to make low-risk, reversible implementation decisions.

For a decision that is:

- local;
- easy to change;
- not visible in the language contract;
- compatible with the architecture;

choose a reasonable default, document it briefly, and continue.

For a decision that would permanently define Arcana's public language semantics, bytecode compatibility, or geometry grammar, do one of the following:

1. implement the smallest provisional version behind a clearly documented version-0 rule; or
2. present 2-3 concrete alternatives with tradeoffs and identify the one you recommend.

Do not block progress over decisions that can be deferred.

## 25. First Work Session: Required Actions

On the first session, perform these actions in order:

1. Read both architecture DOCX files completely.
2. Inspect repository structure, build files, existing tests, and current implementation status.
3. Summarize conflicts or gaps between repository reality and the documents.
4. Create the geometry-native source-model ADR described above.
5. Decide or confirm implementation language/build system.
6. Identify the smallest currently missing executable milestone.
7. Implement that milestone with tests.
8. Run the relevant test suite and any formatter/linter already used by the repository.
9. Produce a short implementation log.
10. Continue to the next milestone if time/context allows rather than stopping after scaffolding.

If the repository is empty, begin with Milestone A and establish the project/workspace plus tests.

## 26. Definition of Done for the Initial Arcana Prototype

The initial prototype is not "done" because a circle can be drawn. It is done when the lower language stack proves the design.

The minimum prototype must demonstrate:

- own Arcana `.mgc` bytecode format;
- assembler;
- disassembler;
- verifier;
- stack VM;
- basic runtime values;
- call frames;
- semantic graph with regions/edges/cyclic port order;
- topology validation;
- topology-aware lowering to HIR;
- HIR/MIR compiler pipeline;
- bytecode emission;
- source/provenance map;
- end-to-end arithmetic;
- function call;
- branching;
- recursion;
- deterministic tests;
- `fib(10) == 55` on Arcana's own VM;
- at least one end-to-end test whose operand meaning depends on drawing-native topology rather than a conventional text parser.

## 27. Deliverables to Maintain

As implementation progresses, maintain these artifacts or their repository-equivalent forms:

- architecture decision records;
- `language-semantic-graph.md` or equivalent specification;
- `bytecode-format.md`;
- `opcode-reference.md`;
- HIR/MIR format notes;
- test fixtures;
- golden disassembly outputs;
- runnable CLI examples;
- known limitations/roadmap.

Do not allow the two original DOCX files to become misleading. If implementation meaningfully supersedes a decision, record the replacement explicitly rather than silently drifting.

## 28. Agent Status Report Format

At the end of each coding cycle, report:

### Implemented

Concrete code/features completed.

### Tests

Commands run and pass/fail status.

### Architecture decisions

Any choices that affect future work, with file/ADR references.

### Deviations from source documents

Any intentional differences from `arcana_compiler_architecture.docx` or `arcana_vm_compiler_runtime_architecture.docx`.

### Known gaps

Bugs, incomplete semantics, missing verifier checks, or temporary shortcuts.

### Next milestone

The next specific executable goal, not a vague roadmap item.

## 29. Immediate Target

Begin now.

Do not work on image recognition, drawing tools, brush input, rendering, or editor UX.

The immediate objective is:

```text
hand-authored Arcana Semantic Graph
        -> validates drawing-native topology
        -> lowers to HIR
        -> lowers to MIR
        -> emits .mgc
        -> passes Arcana verifier
        -> executes on Arcana VM
        -> produces the expected result
```

The first result is `15` from the topology-derived representation of `5 + 10`.

The next major result is `55` from `fib(10)`.

Build the smallest real system that proves those statements, keep every layer inspectable, and leave the repository in a tested state after each iteration.

---

# Notes for the Project Owner

## Why this follows normal language-engineering practice

Arcana is unusual at the source layer, but the plan deliberately becomes conventional after source semantics are resolved. This is a strength. CPython, Lua, Java-like VMs, and many other language implementations separate frontend semantics from executable form. Arcana does the same; its frontend semantic object is simply richer because it preserves topology.

The important distinction is:

```text
NOT:
Drawing -> ordinary Python-like AST -> VM

BUT:
Drawing -> normalized topology -> Arcana Semantic Graph
        -> Arcana-specific semantic lowering
        -> HIR/MIR -> bytecode -> VM
```

That design makes it possible for geometry to be genuinely meaningful without forcing the runtime to manipulate pixels or circles.

## Why Python is a useful reference

Python/CPython is useful because it demonstrates the full lifecycle of a practical high-level language: source representation, semantic processing, bytecode/code metadata, interpreter frames, runtime values, debugging information, and extensive layered testing. Arcana can borrow the discipline while rejecting Python's syntax, dynamic semantics, and historical complexity.

## Why Brainfuck is not the main reference

Brainfuck is excellent for understanding the absolute minimum interpreter loop, but it has too little language structure to guide Arcana's real needs. Arcana will require scopes, functions, types or typed operations, control flow, metadata, diagnostics, bytecode verification, and eventually heap objects. A mature bytecode language such as Python is therefore a better architectural comparison.
