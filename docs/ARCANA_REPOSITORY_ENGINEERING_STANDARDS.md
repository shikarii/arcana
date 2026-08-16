# Arcana Repository Engineering Standards

**Status:** Normative repository-wide engineering policy  
**Audience:** Core contributors, compiler/runtime engineers, tooling authors, AI coding agents, reviewers, release engineers  
**Applies to:** The entire Arcana repository unless a more-specific approved standard explicitly overrides it

---

## 1. Purpose

Arcana is intended to be engineered as a serious, long-lived programming language implementation rather than as a prototype that happens to work.

These standards exist to establish the habits, boundaries, and invariants that are easiest to preserve when they are defined at the beginning and hardest to retrofit after a language ecosystem grows.

The goal is to make Arcana a codebase that could plausibly survive:

- decades of development,
- multiple generations of maintainers,
- a large public package ecosystem,
- backwards-compatibility pressure,
- multiple compiler/runtime implementations,
- external tooling,
- security scrutiny,
- performance-sensitive workloads,
- automated contributors and coding agents,
- and users who expect programs written today to keep working years from now.

When speed of implementation conflicts with architectural integrity, prefer the smallest implementation that preserves the intended architecture.

---

# 2. Normative Language

The keywords below are used deliberately:

- **MUST** — required.
- **MUST NOT** — prohibited.
- **SHOULD** — expected unless there is a documented reason not to.
- **SHOULD NOT** — discouraged unless there is a documented reason.
- **MAY** — optional.
- **EXPERIMENTAL** — explicitly not stable and must not silently become relied upon as stable behavior.

If a change violates a MUST or MUST NOT rule, the pull request must either be corrected or include an approved architecture decision that changes the rule.

---

# 3. Core Engineering Principles

## 3.1 Correctness before cleverness

Arcana is infrastructure.

Compiler and VM code should optimize for:

1. semantic correctness,
2. diagnosability,
3. maintainability,
4. deterministic behavior,
5. testability,
6. performance,
7. implementation cleverness.

A fast compiler that occasionally miscompiles valid programs is broken.

A VM optimization that cannot be validated against a reference execution model is suspect.

---

## 3.2 Make invalid states difficult to represent

Internal data structures SHOULD encode invariants structurally wherever practical.

Prefer:

```text
VerifiedBytecode
```

over:

```text
ByteBuffer + bool is_verified
```

Prefer:

```text
ResolvedSymbolId
```

over passing arbitrary integers with comments explaining what they mean.

Prefer typed IDs and typed handles over raw indices when the implementation language permits them.

---

## 3.3 Every layer owns its semantics

Arcana's intended architecture is approximately:

```text
Drawing / Editor
        ↓
Topological Normalization
        ↓
Arcana Semantic Graph
        ↓
Semantic Analysis / HIR
        ↓
MIR / CFG
        ↓
Bytecode
        ↓
Verifier
        ↓
Arcana VM
```

Each layer MUST expose a clear contract to the next.

Lower layers MUST NOT reach upward to recover information that should have been explicitly lowered.

Higher layers MUST NOT depend on private implementation details of lower layers.

---

## 3.4 The drawing is canonical language syntax

Arcana MUST NOT quietly evolve into a conventional text language with a graphical editor placed on top.

Textual forms MAY exist for:

- tests,
- debugging,
- assembly,
- disassembly,
- IR inspection,
- semantic-graph fixtures,
- serialization,
- interchange,
- tooling,
- bootstrapping.

These forms MUST be described as representations of Arcana structures, not as the hidden "real language" unless the project explicitly changes that design.

The compiler core starts below the image/pixel layer.

---

## 3.5 Determinism is a feature

Given identical:

- source semantics,
- compiler version,
- target,
- flags,
- dependencies,
- environment inputs declared as relevant,

Arcana SHOULD produce byte-for-byte identical build artifacts wherever feasible.

The compiler MUST NOT accidentally depend on:

- hash-map iteration order,
- thread scheduling,
- filesystem enumeration order,
- pointer values,
- wall-clock time,
- random seeds,
- locale,
- host-specific undefined behavior.

If randomness is intentionally used, the seed MUST be explicit and reproducible.

---

# 4. Repository Structure

The repository SHOULD converge toward a structure similar to:

```text
arcana/
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── CODE_OF_CONDUCT.md
├── SECURITY.md
├── CHANGELOG.md
├── ROADMAP.md
├── VERSION
│
├── spec/
│   ├── language/
│   ├── semantic-graph/
│   ├── bytecode/
│   ├── vm/
│   └── compatibility/
│
├── docs/
│   ├── architecture/
│   ├── design/
│   ├── adr/
│   ├── internals/
│   ├── tooling/
│   └── development/
│
├── src/
│   ├── common/
│   ├── diagnostics/
│   ├── semantic_graph/
│   ├── semantic/
│   ├── hir/
│   ├── mir/
│   ├── bytecode/
│   ├── compiler/
│   ├── verifier/
│   ├── vm/
│   ├── runtime/
│   └── platform/
│
├── tools/
│   ├── arcana/
│   ├── arcana-asm/
│   ├── arcana-dis/
│   ├── arcana-verify/
│   ├── arcana-inspect/
│   └── developer-tools/
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── conformance/
│   ├── compile-fail/
│   ├── runtime-fail/
│   ├── golden/
│   ├── regression/
│   └── compatibility/
│
├── fuzz/
├── benchmarks/
├── examples/
├── fixtures/
├── scripts/
└── third_party/
```

Exact names MAY vary, but architectural boundaries MUST remain visible in the filesystem.

A repository where all compiler logic lives in one `compiler/` directory with no pass boundaries is not an acceptable long-term structure.

---

# 5. Dependency Direction

Dependencies SHOULD point downward through the architecture.

A recommended direction is:

```text
common
  ↑
diagnostics
  ↑
semantic_graph
  ↑
semantic / hir
  ↑
mir
  ↑
bytecode
  ↑
compiler backend

bytecode
  ↑
verifier
  ↑
vm/runtime
```

Some shared utilities will necessarily be used by multiple layers, but dependency cycles MUST NOT become normal.

## 5.1 Forbidden architectural shortcuts

Examples of forbidden coupling:

- VM code importing HIR structures.
- Bytecode instructions containing editor UI objects.
- Semantic analysis asking the VM to determine whether an expression is valid.
- Runtime objects depending on compiler AST nodes.
- Diagnostics being constructed from raw UI coordinates instead of stable source provenance.
- Compiler passes directly mutating unrelated earlier-stage representations.
- Tooling duplicating opcode definitions instead of using the canonical opcode schema.

If two layers need to share information, define a stable lower-level representation for that information.

---

# 6. Module Responsibilities

## `common/`

Contains only genuinely cross-cutting primitives.

Examples:

- typed IDs,
- spans/source IDs,
- checked integer helpers,
- deterministic collections,
- serialization helpers,
- result/error primitives.

`common/` MUST NOT become a dumping ground.

---

## `semantic_graph/`

Owns the normalized topology-aware representation produced below the drawing layer.

It may represent concepts such as:

- nodes,
- directed edges,
- regions,
- containment,
- ports,
- cyclic port ordering,
- source provenance,
- topological relations.

It MUST NOT contain VM execution details.

---

## `semantic/`

Owns language-level meaning:

- symbol resolution,
- scopes,
- type checking,
- effect checking,
- legality rules,
- graph semantic validation,
- compile-time diagnostics.

---

## `hir/`

Owns a high-level normalized semantic representation.

HIR should be easy for compiler developers to understand.

Surface/topological oddities should be reduced into explicit semantic concepts here.

---

## `mir/`

Owns explicit execution structure:

- basic blocks,
- control flow,
- temporaries,
- calls,
- branches,
- operations,
- lowered runtime behaviors.

MIR MUST have documented invariants.

---

## `bytecode/`

Owns the bytecode specification and binary encoding.

This module SHOULD contain the canonical definitions from which the following can be generated or validated:

- opcode enum,
- instruction encoding,
- operand descriptions,
- stack effects,
- verifier metadata,
- disassembler metadata,
- documentation tables.

There MUST NOT be several manually maintained opcode lists.

---

## `verifier/`

Owns validation of executable bytecode before execution.

The VM SHOULD be permitted to assume verifier guarantees when executing verified bytecode.

---

## `vm/`

Owns instruction execution and VM machine state.

The VM MUST NOT reinterpret source-language syntax.

---

## `runtime/`

Owns services required by executing Arcana programs, such as:

- strings,
- arrays,
- heap objects,
- garbage collection,
- exceptions,
- native interfaces,
- runtime type metadata.

The division between `vm/` and `runtime/` SHOULD remain deliberate.

---

# 7. Public APIs vs Internal APIs

Every module SHOULD distinguish between:

- public/stable interfaces,
- internal interfaces,
- experimental interfaces.

Internal implementation convenience MUST NOT accidentally become ecosystem API.

A public API requires:

- documentation,
- tests,
- stability expectations,
- versioning policy,
- review appropriate to its compatibility cost.

Once third-party code depends on an API, changing it becomes much more expensive.

---

# 8. Single Sources of Truth

The repository MUST avoid duplicated semantic definitions.

Examples of information that should have one canonical definition:

- opcode numbers,
- opcode names,
- stack effects,
- binary format versions,
- primitive type IDs,
- well-known runtime symbols,
- diagnostic codes,
- feature gates,
- bytecode section identifiers,
- target triples,
- language version identifiers.

When possible, generate secondary representations.

Example:

```text
opcode schema
    ├── VM decode table
    ├── verifier table
    ├── assembler parser table
    ├── disassembler table
    └── documentation
```

This is preferable to five hand-maintained switch statements.

---

# 9. Code Style

## 9.1 Optimize for local readability

A contributor should be able to understand a function without reconstructing the entire compiler.

Prefer:

- small units with explicit contracts,
- descriptive names,
- shallow control flow,
- typed domain objects,
- explicit error handling.

Avoid:

- enormous "god" functions,
- hidden mutation,
- clever metaprogramming without strong justification,
- unexplained numeric constants,
- boolean parameters whose meaning is unclear at the call site.

---

## 9.2 Name things by domain meaning

Prefer:

```text
BlockId
FunctionId
ConstantPoolIndex
RegionId
SourceElementId
StackDepth
```

over:

```text
int id
size_t index
int thing
bool mode
```

Names should reflect the abstraction being represented, not merely its machine type.

---

## 9.3 Comments explain why

Comments SHOULD explain:

- invariants,
- non-obvious reasoning,
- compatibility constraints,
- tricky algorithms,
- intentional deviations,
- security assumptions.

Comments SHOULD NOT narrate obvious code.

Bad:

```text
increment i
```

Good:

```text
Instruction offsets are encoded relative to the end of the current
instruction, matching the bytecode format specification.
```

---

## 9.4 TODOs are tracked work

A TODO MUST include enough information to be actionable.

Preferred:

```text
TODO(#184): Preserve region provenance through inlining.
```

Avoid permanent anonymous TODOs.

---

# 10. Function and File Size

There is no absolute line-count law, but size is a design signal.

A function SHOULD generally perform one conceptual operation.

Large files SHOULD be split by domain responsibility, not arbitrarily by line count.

Reviewers should question:

- functions above roughly 100–150 lines,
- files above roughly 1,000–1,500 lines,
- classes/modules that own unrelated concerns.

Exceptions are reasonable for:

- generated code,
- declarative tables,
- exhaustive protocol definitions.

---

# 11. Error Handling

Compiler/runtime infrastructure MUST use explicit error paths.

## 11.1 Never silently recover from internal corruption

Internal invariant failures SHOULD fail loudly in debug/development builds.

Malformed user programs are expected errors.

Malformed compiler state is a bug.

These categories MUST NOT be conflated.

---

## 11.2 User errors are not crashes

Invalid Arcana source MUST produce diagnostics rather than:

- process crashes,
- assertions,
- null dereferences,
- stack traces intended for compiler developers.

---

## 11.3 Bytecode is hostile input

The bytecode loader and verifier MUST treat input as untrusted.

Malformed bytecode MUST NOT cause:

- out-of-bounds reads,
- arbitrary memory access,
- integer overflow leading to unsafe behavior,
- unchecked stack underflow,
- invalid control-flow targets,
- forged references,
- process compromise.

Even if the initial compiler only emits trusted bytecode, external bytecode will eventually exist.

---

# 12. Assertions and Invariants

Assertions are appropriate for internal invariants.

Examples:

```text
MIR block IDs are valid.
SSA value IDs are in range.
Verified bytecode stack depth matches verifier output.
```

Assertions MUST NOT be used to validate normal user input.

Important invariants SHOULD be documented next to the type or pass that owns them.

---

# 13. Compiler Pass Discipline

Every compiler pass MUST define:

1. accepted input representation,
2. required input invariants,
3. produced output representation,
4. guaranteed output invariants,
5. diagnostics/errors it may emit,
6. whether it mutates or constructs data,
7. deterministic ordering guarantees.

A pass SHOULD resemble:

```text
InputRepresentation -> Result<OutputRepresentation, Diagnostics>
```

rather than an operation that mutates global compiler state invisibly.

---

# 14. IR Design Rules

HIR and MIR are long-lived architectural assets.

They MUST NOT be treated as temporary structs that happen to make the current feature work.

For each IR:

- document every node/instruction,
- define ownership/lifetime rules,
- define ID stability expectations,
- define whether ordering is semantic,
- define valid/invalid states,
- provide pretty-printing,
- provide validation,
- provide tests.

Every IR SHOULD have a verifier/debug validator.

Example:

```text
verify_hir(module)
verify_mir(function)
```

Compiler debug builds SHOULD run validation between major passes where practical.

---

# 15. Source Provenance

Every meaningful compiler representation SHOULD preserve enough provenance to report errors against the original Arcana drawing.

Stable provenance is more important than raw pixel coordinates.

Prefer:

```text
SourceElementId
RegionId
ConnectionId
GlyphId
```

with editor/source-map lookup information.

Optimizations and lowering passes SHOULD preserve or combine provenance.

A runtime exception SHOULD eventually be traceable back to the semantic source element that generated the relevant code.

---

# 16. Bytecode Design Standards

Bytecode is a compatibility boundary.

Treat it as a real format from the beginning.

The bytecode specification MUST define:

- file magic,
- format version,
- endianness,
- integer encoding,
- section layout,
- opcode encoding,
- operand encoding,
- function representation,
- constant representation,
- jump semantics,
- stack semantics,
- validation rules,
- debug metadata behavior,
- unknown-section behavior,
- compatibility behavior.

---

## 16.1 Opcode stability

Once a released bytecode version is declared stable:

- opcode numeric IDs MUST NOT be silently repurposed,
- operand meanings MUST NOT change without a version change,
- removed instructions SHOULD remain reserved,
- compatibility expectations MUST be documented.

---

## 16.2 Stack effects

Every stack instruction MUST have a precisely documented stack effect.

Examples:

```text
ADD_I64:     ..., a, b -> ..., result
POP:         ..., x -> ...
CALL n:      ..., callee, arg0 ... argN-1 -> ..., result
RETURN:      ..., result -> caller
```

The verifier SHOULD derive or validate stack behavior from the canonical opcode definition.

---

# 17. Bytecode Verification

Verification is mandatory architecture, not an optional hardening pass.

The verifier SHOULD validate at minimum:

- valid file structure,
- valid sections,
- valid constant indices,
- valid function indices,
- valid opcode encodings,
- valid operand widths,
- valid jump targets,
- stack underflow,
- stack merge consistency,
- local index validity,
- type constraints where bytecode carries static type information,
- call arity where statically knowable,
- exception table validity,
- region metadata validity if executable.

The VM SHOULD execute only verified bytecode except in explicitly marked developer/testing modes.

---

# 18. VM Standards

## 18.1 The VM is not the compiler

The VM executes bytecode semantics.

It MUST NOT contain hacks that recognize patterns corresponding to source-language constructs.

Example of a bad shortcut:

```text
if this bytecode sequence looks like a visual loop, do something special
```

The compiler owns lowering.

---

## 18.2 Specify machine state explicitly

VM state MUST be conceptually documented.

At minimum:

- instruction pointer,
- operand stack or registers,
- call frames,
- globals/modules,
- heap,
- exception state,
- runtime configuration.

---

## 18.3 Reference interpreter behavior

For important semantics, maintain a simple implementation that prioritizes obvious correctness over optimization.

Optimized execution paths SHOULD be testable against reference behavior.

If a JIT is added later, it should be possible to compare:

```text
interpreter result == JIT result
```

for randomized/conformance programs.

---

# 19. Memory Management

Memory safety bugs in a language runtime are ecosystem-level bugs.

The runtime MUST have explicit ownership rules for:

- VM values,
- heap objects,
- interned objects,
- native handles,
- stack references,
- global roots,
- compiler-owned temporary structures.

If garbage collection is used, the root model MUST be documented.

Every GC change requires tests covering:

- object reachability,
- cycles,
- nested calls,
- exceptions,
- collections,
- closures,
- native/runtime references.

---

# 20. Undefined and Unspecified Behavior

Arcana SHOULD minimize undefined behavior at the language level.

Every behavior should ideally be one of:

- specified,
- implementation-defined,
- unspecified but bounded,
- compile-time error,
- runtime error.

True undefined behavior SHOULD be rare and strongly justified.

Do not accidentally expose implementation-language undefined behavior as Arcana semantics.

Examples requiring explicit decisions:

- integer overflow,
- division by zero,
- invalid shifts,
- NaN behavior,
- map iteration order,
- uninitialized values,
- concurrent mutation,
- stack exhaustion.

---

# 21. Compatibility Policy

Compatibility must be designed before it becomes painful.

Arcana should separately version:

- language semantics,
- bytecode format,
- runtime ABI,
- standard library,
- package format,
- tool protocol APIs.

These versions MAY initially move together, but they are conceptually distinct.

---

## 21.1 Backwards compatibility

Once Arcana reaches stable releases, breaking user programs requires deliberate process.

Changes should be classified:

```text
Bug fix
Compatible extension
Deprecation
Behavioral change
Source-breaking change
Bytecode-breaking change
ABI-breaking change
```

Breaking changes require:

- rationale,
- migration path,
- release-note visibility,
- compatibility-version consideration.

---

## 21.2 No accidental language features

If users can observe behavior, it may become depended upon.

Therefore avoid exposing accidental semantics such as:

- deterministic object addresses,
- arbitrary hash iteration ordering,
- undocumented reflection fields,
- incidental bytecode shapes,
- compiler-generated private names.

---

# 22. Feature Development Process

A significant language or VM feature SHOULD move through:

```text
idea
  ↓
design note
  ↓
prototype if necessary
  ↓
architecture review
  ↓
specification
  ↓
implementation
  ↓
conformance tests
  ↓
documentation
  ↓
stabilization
```

Not every small feature needs bureaucracy, but language semantics should not emerge accidentally from an implementation PR.

---

# 23. Architecture Decision Records

Major decisions SHOULD receive an ADR under:

```text
docs/adr/
```

Example:

```text
0001-stack-based-vm.md
0002-bytecode-versioning.md
0003-semantic-graph-as-compiler-boundary.md
0004-region-containment-defines-lexical-scope.md
```

Each ADR should include:

- context,
- decision,
- alternatives considered,
- consequences,
- status.

Do not rewrite history when a decision changes. Supersede the earlier ADR.

---

# 24. Language Specification Discipline

The implementation is not the only definition of Arcana.

The project SHOULD maintain a written language specification.

A feature is not complete when "the compiler accepts it."

A feature is complete when:

- behavior is specified,
- compiler implements it,
- VM/runtime implements required semantics,
- tests validate it,
- diagnostics are reasonable,
- documentation teaches it.

---

# 25. Testing Philosophy

A programming language requires unusually strong testing.

No meaningful compiler/runtime feature is complete without tests.

Use multiple test classes because each catches different failures.

---

## 25.1 Unit tests

For:

- encoders/decoders,
- symbol tables,
- graph utilities,
- individual compiler transformations,
- runtime primitives.

---

## 25.2 Integration tests

For complete flows such as:

```text
Semantic Graph
  -> compiler
  -> bytecode
  -> verifier
  -> VM
  -> expected output
```

---

## 25.3 Compile-fail tests

Invalid programs SHOULD be first-class tests.

Test:

- rejection,
- diagnostic code,
- relevant source provenance,
- message quality where stable.

---

## 25.4 Runtime-fail tests

Programs expected to fail at runtime should test:

- error category,
- stack trace,
- source mapping,
- VM cleanup behavior.

---

## 25.5 Golden tests

Golden files are appropriate for:

- disassembly,
- diagnostics,
- IR dumps,
- binary layouts,
- source maps.

Golden updates MUST be reviewed as semantic changes, not blindly regenerated.

---

## 25.6 Regression tests

Every fixed compiler or VM bug SHOULD receive a regression test whenever practical.

The test should reproduce the original failure.

---

## 25.7 Conformance tests

Maintain tests that describe Arcana semantics independently of a particular implementation.

The long-term goal is that a second Arcana implementation could run the same conformance suite.

---

# 26. Differential Testing

Where two implementations of the same semantics exist, compare them automatically.

Examples:

```text
HIR interpreter vs bytecode VM
unoptimized MIR vs optimized MIR
interpreter vs future JIT
old compiler vs new compiler for compatible programs
```

Differential testing is especially valuable for compiler optimization.

---

# 27. Property-Based Testing

Use generated tests for structural invariants.

Examples:

- encode -> decode round trips,
- serialize -> deserialize round trips,
- disassemble -> assemble round trips where defined,
- CFG transformations preserve validity,
- optimizer preserves observable behavior,
- random valid bytecode never crashes verifier,
- random invalid bytecode never bypasses required checks.

---

# 28. Fuzzing

Compiler and runtime inputs should be fuzzed continuously.

High-value fuzz targets include:

- bytecode decoder,
- verifier,
- assembler,
- semantic-graph decoder,
- parser for any interchange format,
- runtime object serialization,
- optimizer passes,
- diagnostic rendering.

A malformed file causing a crash is a bug even if normal tools would never generate that file.

---

# 29. Performance Standards

Performance work requires measurement.

No optimization should be accepted because it "should be faster."

Require benchmarks for meaningful performance claims.

Track separately:

- compiler startup,
- compile throughput,
- peak compiler memory,
- bytecode size,
- VM startup,
- runtime throughput,
- runtime memory,
- GC pause time,
- standard-library performance.

---

## 29.1 Performance budgets

As Arcana matures, define budgets for common operations.

Regressions above a chosen threshold SHOULD require explicit approval or explanation.

---

## 29.2 Do not optimize away architecture

Early Arcana should prefer:

```text
correct clean pass
```

over:

```text
four fused passes sharing mutable hidden state
```

Optimize after profiling.

---

# 30. Security Standards

Treat a language runtime as security-sensitive infrastructure.

At minimum:

- bounds-check untrusted binary data,
- avoid unchecked arithmetic in sizes/offsets,
- validate all external indices,
- fuzz binary decoders,
- document unsafe code,
- isolate native interfaces,
- avoid shell injection in build tooling,
- pin or audit critical dependencies,
- provide a vulnerability reporting policy.

Every use of unsafe implementation-language features SHOULD have a documented safety invariant.

---

# 31. Unsafe Code Policy

If the implementation language permits unsafe operations:

- unsafe code MUST be localized,
- unsafe blocks MUST document required invariants,
- safe wrappers SHOULD contain the unsafe implementation,
- unsafe code SHOULD have focused tests,
- unnecessary unsafe code is prohibited.

The compiler being "low level" is not justification for casual unsafe code.

---

# 32. Concurrency

Concurrency MUST be explicit.

Compiler parallelism MUST NOT change observable output.

Shared mutable global state SHOULD be avoided.

If caches are shared:

- thread-safety must be documented,
- invalidation rules must be documented,
- deterministic behavior must be preserved.

Runtime concurrency semantics must be specified before users can rely on them.

---

# 33. Global State

Avoid hidden process-global state.

Especially avoid global mutable:

- compiler options,
- intern tables,
- diagnostic sinks,
- current module,
- VM state,
- source managers,
- caches.

Prefer explicit context/session objects.

Example:

```text
CompilerSession
VmInstance
SourceDatabase
DiagnosticContext
```

This enables:

- multiple compilers in one process,
- tests in parallel,
- embedding,
- IDE tooling,
- reproducible builds.

---

# 34. Diagnostics Standards

Diagnostics are part of the language UX.

Every diagnostic SHOULD have:

- severity,
- stable diagnostic code,
- primary source element,
- concise message,
- contextual notes when useful,
- actionable help when possible.

Example:

```text
ARC-TYPE-0012
Cannot connect `String` output to `Int` input.
```

Stable codes make documentation, tests, IDE integration, and search easier.

---

## 34.1 Never expose compiler internals to users

Bad:

```text
failed HIR node lowering at index 42
```

Good:

```text
This connection supplies two values, but the target rune accepts one.
```

Internal IDs MAY be included in debug logs, not normal diagnostics.

---

# 35. Logging and Tracing

Compiler and VM tracing SHOULD be structured.

Useful categories may include:

```text
semantic
hir
mir
bytecode
verifier
vm
gc
loader
ffi
```

Normal executions MUST NOT emit debug logging.

Developer tracing SHOULD be enableable without recompilation when practical.

---

# 36. Observability for Compiler Development

The following tools SHOULD exist early:

```text
--dump-semantic-graph
--dump-hir
--dump-mir
--emit-bytecode
--disassemble
--verify
--trace-vm
```

A compiler that cannot show its intermediate representations is unnecessarily hard to debug.

---

# 37. Generated Code

Generated code MUST:

- clearly state that it is generated,
- identify its generator,
- not be hand-edited,
- be reproducible.

Where practical, CI SHOULD verify generated files are up to date.

---

# 38. Build System Standards

The build MUST have a documented clean path from fresh checkout to working tools.

A new contributor should not need tribal knowledge.

Prefer:

```text
configure
build
test
```

or an equally simple project-native flow.

The repository SHOULD provide wrapper scripts only when they add consistency rather than hiding basic tooling.

---

# 39. Reproducible Builds

Release artifacts SHOULD eventually be reproducible.

Avoid embedding:

- build timestamps,
- absolute local paths,
- random UUIDs,
- hostnames,

unless explicitly requested.

Debug metadata requiring paths SHOULD support path remapping.

---

# 40. Dependencies

Every dependency carries long-term cost.

New dependencies SHOULD be evaluated for:

- maintenance health,
- license,
- security history,
- portability,
- transitive dependency size,
- whether the functionality is core enough to implement locally,
- ecosystem lock-in.

Do not reimplement cryptography, compression, Unicode databases, or similarly specialized infrastructure casually.

Do not import a large framework to save fifty lines of straightforward code.

---

# 41. Third-Party Code

Third-party source MUST be isolated under a clearly identified area.

Never casually copy code from external projects into Arcana without:

- license compatibility,
- attribution where required,
- provenance,
- maintenance ownership.

---

# 42. Platform Abstraction

OS-specific behavior SHOULD live behind a narrow platform layer.

Core compiler and VM logic SHOULD NOT be littered with:

```text
if Windows
if Linux
if macOS
```

Centralize:

- file mapping,
- dynamic libraries,
- clocks,
- threads,
- virtual memory,
- terminal behavior,
- process invocation.

---

# 43. Portability

Core formats MUST use explicit-width integer types.

Never assume:

- pointer size,
- native endianness,
- `sizeof(long)`,
- host path conventions,
- host newline conventions.

Bytecode semantics are Arcana-defined, not host-ABI-defined.

---

# 44. Standard Library Boundary

Keep language/runtime primitives distinct from standard-library conveniences.

A feature should not become a VM opcode merely because it is useful.

VM primitives should justify themselves through:

- semantic necessity,
- performance necessity,
- privileged runtime access,
- widespread implementation value.

Otherwise prefer library implementation.

---

# 45. Native/FFI Boundary

The FFI is a security, portability, and compatibility boundary.

It SHOULD be introduced deliberately rather than as an early shortcut.

Native calls MUST NOT silently bypass:

- GC rooting requirements,
- exception conventions,
- runtime ownership rules,
- capability/security constraints.

---

# 46. Serialization Formats

Every persistent format MUST be versioned.

This includes:

- `.mgc` bytecode,
- semantic graph files,
- caches,
- package metadata,
- debug data.

Readers SHOULD reject unsupported versions clearly.

Never assume an internal serialization can remain unversioned because "only our tools use it."

---

# 47. Caching

Compiler caches are optimizations, never correctness dependencies.

Deleting all caches MUST produce a correct build.

Cache keys must account for every input that changes output.

A stale cache producing incorrect code is a compiler bug.

---

# 48. Incremental Compilation

Incrementality SHOULD be layered on top of deterministic pure-ish compiler stages.

Do not contaminate semantic correctness with cache state.

Prefer explicit dependency tracking.

---

# 49. Optimization Passes

Every optimization MUST preserve observable program behavior as defined by the language specification.

Each optimization SHOULD include:

- documented transformation,
- preconditions,
- tests,
- examples,
- differential/property tests where feasible.

Optimizations MUST NOT rely on behavior the language has not declared undefined or unspecified.

---

# 50. Debug vs Release Behavior

Debug builds MAY contain expensive validation.

Release builds MAY omit internal checks proven redundant after verification.

User-visible language semantics MUST NOT change between debug and release builds.

---

# 51. Version Control Standards

Commits SHOULD be:

- focused,
- buildable where practical,
- reviewable,
- descriptive.

Avoid combining:

- mechanical rename,
- architecture refactor,
- feature implementation,

in one enormous diff when they can be separated.

---

# 52. Pull Request Standards

A meaningful PR SHOULD explain:

- what changed,
- why,
- architecture impact,
- user-visible behavior,
- tests added,
- compatibility impact,
- performance impact if relevant,
- security implications if relevant.

Large semantic changes SHOULD link an issue, design document, or ADR.

---

# 53. Review Standards

Reviewers are responsible for architecture, not just syntax.

Review should ask:

- Is this behavior specified?
- Is this the correct layer?
- Is the abstraction durable?
- Are invariants explicit?
- Is this deterministic?
- Can malformed input break it?
- Is there adequate test coverage?
- Does it create compatibility debt?
- Is there a simpler implementation?

---

# 54. CI Requirements

Main should remain healthy.

CI SHOULD eventually include:

```text
format check
lint/static analysis
debug build
release build
unit tests
integration tests
conformance tests
compile-fail tests
runtime-fail tests
sanitizers
fuzz smoke tests
documentation validation
generated-file validation
cross-platform builds
```

A failing main branch should be treated as urgent infrastructure damage.

---

# 55. Sanitizers and Dynamic Analysis

Where supported, regularly run:

- address sanitizer,
- undefined behavior sanitizer,
- memory sanitizer where feasible,
- thread sanitizer where feasible,
- leak detection.

Runtime/compiler infrastructure benefits disproportionately from these tools.

---

# 56. Warnings

Project-owned code SHOULD compile warning-free under the project's supported toolchain.

New warnings SHOULD fail CI once the warning policy is mature.

Suppressions require narrow scope and explanation.

---

# 57. Documentation Standards

Every important subsystem should answer:

1. What does this subsystem do?
2. What are its inputs?
3. What are its outputs?
4. What invariants does it require?
5. What invariants does it guarantee?
6. Which other subsystem owns adjacent responsibilities?
7. How is it tested?

Architecture documentation should live close enough to the code that contributors can find it.

---

# 58. No Tribal Knowledge

If a contributor must know something important to safely change the repository, that knowledge belongs in:

- code comments,
- a specification,
- a design document,
- an ADR,
- developer documentation.

"Ask the original author" is not a sustainable architecture.

---

# 59. Examples Are Tests

Important documentation examples SHOULD be executable or validated where practical.

Do not allow docs to teach syntax or semantics that the compiler no longer implements.

---

# 60. Release Discipline

A release SHOULD be produced from a clean, tagged source state.

Release artifacts SHOULD include:

- version,
- changelog,
- compatibility notes,
- checksums,
- supported platforms,
- known limitations.

The project SHOULD be able to reconstruct how any official binary was produced.

---

# 61. Deprecation Policy

Stable features should not disappear unexpectedly.

A deprecation SHOULD include:

- what is deprecated,
- replacement,
- reason,
- first deprecated version,
- expected removal window if known.

Compiler diagnostics SHOULD make migration straightforward.

---

# 62. Experimental Features

Experimental features MUST be clearly marked.

Prefer explicit feature gates.

Experimental behavior MUST NOT silently become stable through widespread accidental availability.

Graduation to stable requires:

- semantic decision,
- specification,
- tests,
- compatibility review.

---

# 63. Package Ecosystem Future-Proofing

Even before Arcana has packages, avoid decisions that make a package ecosystem difficult later.

Reserve room for:

- package identity,
- semantic versions,
- dependency locking,
- reproducible resolution,
- checksums,
- registries/mirrors,
- offline builds,
- vendoring,
- package metadata,
- build isolation.

Do not conflate module names with globally unique package identity.

---

# 64. Tooling Protocols

IDE/editor tooling should communicate through explicit versioned protocols where possible.

Do not make third-party tooling scrape human-readable compiler output.

Prefer structured output for:

- diagnostics,
- semantic graph information,
- symbol queries,
- compilation metadata.

Human-readable output can be rendered from structured data.

---

# 65. Stable IDs

Stable IDs are especially important for Arcana because source syntax is graphical.

Editor/compiler integration SHOULD use stable semantic identifiers rather than coordinates.

Moving a rune 20 pixels SHOULD NOT make it appear to the compiler as an unrelated new source element when identity can be preserved.

This improves:

- diagnostics,
- incremental compilation,
- source maps,
- refactoring,
- debugging,
- version control tooling.

---

# 66. Graph Canonicalization

Because Arcana source is topology-aware, canonicalization rules MUST be explicit.

The semantic representation should distinguish:

- semantically meaningful ordering,
- purely visual ordering,
- meaningful crossings,
- incidental crossings,
- containment,
- adjacency,
- direction,
- port ordering.

Two visually different drawings that are semantically equivalent SHOULD normalize to the same semantic structure where the language defines them as equivalent.

---

# 67. Geometry Tolerance Is an Editor Concern

The compiler MUST NOT depend on imprecise pixel measurements such as:

```text
angle == 37.0 degrees
distance == 104 pixels
```

The editor/normalizer should convert fuzzy drawing input into discrete semantic relations.

Examples:

```text
inside region
port slot 2
clockwise after edge X
connected
crosses boundary
```

This preserves human-friendly drawing while keeping compiler semantics deterministic.

---

# 68. One-Way Lowering

Lowering SHOULD progressively remove higher-level concepts.

Example:

```text
topological containment
        ↓
lexical scope
        ↓
resolved symbol access
        ↓
local/capture operation
        ↓
bytecode load
```

Do not carry every high-level representation into the VM "just in case."

Preserve source provenance separately.

---

# 69. Debug Metadata Is Not Execution Semantics

Source maps, original graph IDs, visual coordinates, names, and debug annotations SHOULD be separable from executable semantics unless the language explicitly defines them as runtime-visible.

A stripped executable should remain semantically valid when debug information is removed.

---

# 70. Test Every Boundary

Each architectural boundary should have direct tests.

Examples:

```text
Semantic Graph -> HIR
HIR -> MIR
MIR -> bytecode
bytecode -> decode
bytecode -> verify
verified bytecode -> VM behavior
```

End-to-end tests alone are insufficient because they make failures difficult to localize.

---

# 71. Golden Rule for Bugs

When a bug crosses an abstraction boundary, fix it at the layer that owns the violated invariant.

Do not patch the symptom downstream.

Example:

If malformed MIR reaches bytecode generation, ask why MIR validation allowed it.

Do not merely add a special case to the bytecode emitter.

---

# 72. No "Temporary" Architecture Without Containment

Temporary shortcuts have a habit of becoming permanent.

A temporary implementation MAY be accepted if it:

- is isolated behind the intended interface,
- has a tracked replacement,
- does not leak into public semantics,
- does not force downstream code to depend on it.

Bad temporary shortcut:

```text
VM directly executes Semantic Graph nodes.
```

Acceptable bootstrap shortcut:

```text
A simple SemanticGraphInterpreter exists as a separate reference tool.
```

---

# 73. Reference Implementations Are Valuable

Simple, obviously correct implementations should be preserved when they provide an oracle for more sophisticated systems.

Potential examples:

- semantic graph evaluator,
- naïve garbage collector,
- unoptimized bytecode compiler,
- baseline interpreter.

Do not delete a useful correctness oracle solely because a faster path exists.

---

# 74. Keep the Bootstrap Story Deliberate

Arcana may eventually be implemented partly in Arcana.

Do not prematurely self-host.

A self-hosting compiler should happen when:

- the language is stable enough,
- the runtime is reliable enough,
- bootstrapping is reproducible,
- the maintenance benefit is real.

The initial implementation language should remain supported long enough to preserve a trusted bootstrap path.

---

# 75. Language Evolution Requires Restraint

The most successful languages accumulate users faster than they accumulate permission to redesign themselves.

Before adding syntax or semantics, ask:

- Can this be a library?
- Can this be tooling?
- Is it orthogonal to existing concepts?
- Is there one clear meaning?
- Can it be implemented by multiple runtimes?
- Will we regret supporting it forever?

Language features have effectively infinite maintenance lifetimes.

---

# 76. Compatibility Beats Aesthetic Purity After Stability

Before 1.0, Arcana can make aggressive corrections.

After stability, changing a slightly awkward feature may be worse than keeping it.

A mature language belongs partly to its users.

Plan the pre-1.0 period accordingly.

---

# 77. Repository Rules for AI Coding Agents

AI agents MAY implement substantial portions of Arcana, but they MUST follow the same engineering standards as human contributors.

Agents MUST:

1. Read relevant architecture/specification documents before changing a subsystem.
2. Inspect existing code before inventing parallel abstractions.
3. Preserve repository layering.
4. Prefer modifying the canonical definition over duplicating data.
5. Run relevant tests after changes.
6. Add tests for new behavior.
7. Update documentation when architecture or user-visible semantics change.
8. Avoid broad unrelated refactors while implementing a focused feature.
9. Report assumptions.
10. Report unresolved architecture conflicts rather than hiding them.
11. Never silently weaken verification, validation, or error handling to make tests pass.
12. Never replace a designed subsystem with a shortcut merely because it is easier to generate.
13. Preserve backwards compatibility unless explicitly instructed otherwise.
14. Keep generated and handwritten code clearly separated.
15. Leave the repository in a buildable state whenever practical.

---

# 78. AI Agent Change Procedure

Before coding, an agent SHOULD perform:

```text
1. Read architecture/spec
2. Inspect repository tree
3. Locate canonical types and definitions
4. Identify affected dependency boundaries
5. State implementation plan
6. Implement smallest coherent change
7. Add/update tests
8. Run formatter/linter
9. Run targeted tests
10. Run broader relevant tests
11. Inspect diff for unrelated changes
12. Summarize behavior and remaining work
```

For small obvious changes, steps may be compressed, but none should be conceptually ignored.

---

# 79. Agent Anti-Patterns

Agents MUST NOT:

- create `*_new`, `*_v2`, or duplicate systems to avoid understanding existing code,
- add generic utility frameworks without demonstrated repeated need,
- swallow errors,
- replace typed structures with dictionaries/maps for convenience,
- make everything public,
- make everything nullable/optional to avoid modeling invariants,
- disable tests,
- weaken assertions,
- remove validation,
- rewrite large subsystems unless the task truly requires it,
- invent incompatible file formats without versioning,
- introduce dependencies casually,
- use textual Arcana syntax as the real implementation path unless explicitly approved.

---

# 80. Code Ownership Mindset

Even if one person owns the repository today, write code as though another compiler engineer will maintain it five years from now.

Every subsystem should be understandable without access to:

- chat history,
- private notes,
- the original author's memory,
- an AI conversation that produced it.

The repository itself is the durable source of truth.

---

# 81. Definition of Done

A compiler/runtime feature is not done merely because the happy path works.

A substantial feature is done when applicable items below are satisfied:

- semantics are defined,
- correct architectural layer is used,
- implementation exists,
- invariants are documented,
- normal tests exist,
- invalid-input tests exist,
- diagnostics exist,
- debug/inspection tooling understands it,
- serialization/versioning concerns are handled,
- compatibility impact is understood,
- documentation is updated,
- benchmarks exist if performance-sensitive,
- fuzzing is updated if it expands an untrusted input surface.

---

# 82. Initial Quality Gates for Arcana

Before merging to the primary branch, the project should move toward requiring:

```text
[ ] Project formats cleanly
[ ] Static analysis passes
[ ] Project builds in debug mode
[ ] Project builds in release mode
[ ] Unit tests pass
[ ] Integration tests pass
[ ] Conformance tests pass
[ ] Compile-fail tests pass
[ ] Verifier tests pass
[ ] VM runtime-fail tests pass
[ ] Generated files are current
[ ] No new undocumented warnings
[ ] New behavior has tests
[ ] Architecture docs updated when necessary
```

As the project matures, add sanitizers, fuzzing, cross-platform CI, compatibility suites, and performance regression gates.

---

# 83. Recommended Early Repository Files

The repository should establish these early:

```text
README.md
CONTRIBUTING.md
SECURITY.md
CHANGELOG.md
ROADMAP.md
CODE_OF_CONDUCT.md

docs/architecture/overview.md
docs/development/building.md
docs/development/testing.md
docs/development/debugging.md

spec/language/README.md
spec/semantic-graph/README.md
spec/bytecode/README.md
spec/vm/README.md

docs/adr/README.md

benchmarks/README.md
fuzz/README.md
```

This may feel excessive for a tiny project, but lightweight placeholders make the intended structure clear before conventions fragment.

---

# 84. Recommended Compiler Invariant Checks

The implementation should eventually expose developer checks similar to:

```text
validate_semantic_graph()
validate_hir()
validate_mir()
validate_bytecode_module()
verify_bytecode()
validate_runtime_heap()
```

These checks should be cheap enough to use heavily in tests and debug builds.

---

# 85. Recommended Toolchain UX

A mature Arcana repository should aim for commands conceptually similar to:

```text
arcana build
arcana run
arcana check

arcana-asm
arcana-dis
arcana-verify
arcana-inspect

arcana test
arcana fmt
```

Exact names are not binding.

The important rule is that developer tooling should expose the language implementation's layers instead of hiding them.

---

# 86. What Not to Lock Down Too Early

Some decisions should remain intentionally flexible while Arcana is young.

Do not prematurely stabilize:

- package registry protocol,
- JIT ABI,
- native extension ABI,
- object memory layout,
- optimization pipeline,
- standard-library breadth,
- graphical editor file format beyond necessary versioning,
- every possible topology semantic.

Stabilize foundations first:

- semantic meaning,
- layer boundaries,
- bytecode validity model,
- VM execution semantics,
- source identity/provenance model.

---

# 87. Pre-1.0 Philosophy

Before 1.0:

- correctness matters more than compatibility,
- but migrations should still be deliberate,
- old experimental behavior may change,
- version formats anyway,
- record major design decisions,
- avoid creating unnecessary public contracts.

Use the pre-1.0 period to remove architectural mistakes while the ecosystem is small.

---

# 88. Post-1.0 Philosophy

After 1.0:

- source compatibility becomes a major product feature,
- bytecode/version compatibility must be explicit,
- deprecations require migration periods,
- behavior changes require caution,
- ecosystem tooling becomes part of the compatibility surface,
- performance regressions become user-visible regressions.

A successful language has less freedom to redesign itself.

---

# 89. The "Next Python" Standard

If Arcana ever becomes a language used at Python-like scale, the most valuable decisions made at the beginning will not be clever syntax or micro-optimizations.

They will be:

- clear semantics,
- boring, dependable infrastructure,
- excellent diagnostics,
- strict layer boundaries,
- a versioned executable format,
- deterministic compilation,
- aggressive testing,
- a trustworthy verifier,
- stable tooling interfaces,
- documented decisions,
- compatibility discipline,
- and a repository future contributors can understand.

Write today's code as though millions of programs may eventually depend on its behavior.

---

# 90. Final Repository Rule

When deciding between:

```text
"It works right now."
```

and:

```text
"This establishes the correct long-term contract."
```

Arcana SHOULD prefer the second, provided the implementation remains proportionate to the project's current stage.

The repository should be allowed to start small.

It should not be allowed to start careless.
