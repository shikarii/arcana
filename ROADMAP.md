# Arcana Roadmap

## Complete: v0 prototype

The v0 prototype proves the geometry-native design by compiling topology-aware
semantic graphs through a full pipeline to a stack-based VM with 66 opcodes,
mark-sweep GC, and a three-tier concurrency model.

### Core infrastructure
- 66-opcode bytecode format (`.mgc`), assembler, disassembler
- Independent bytecode verifier (operand bounds, stack consistency)
- Stack VM with tagged values, 256 call frames, 1024-slot stack
- Mark-sweep garbage collector with thread-safe allocation
- Semantic graph library with validation
- HIR/MIR intermediate representations
- Topology-aware compiler with constant folding
- Structured diagnostics with stable error codes
- Reference interpreter for differential testing
- Multi-platform CI (Linux, macOS, Windows)

### Language features
- Functions, closures (Lua-style upvalues), recursion
- Branching (if/else), while loops, topology-derived cycles
- Strings, arrays, maps, records
- Type casts (i64, f64, string), bitwise operations
- Exception handling (try/catch/throw)
- Short-circuit boolean evaluation (and/or)
- 9 intrinsics (print, clock, type, assert, tostring, input, len, push, keys)

### Concurrency
- Coroutines (cooperative yield/resume)
- OS threads with argument passing
- Mutexes (lock/unlock)
- Buffered channels (send/recv with condition variables)

### Testing (192 tests)
- L0 challenge corpus: 12 machine sanity tests
- L1 challenge corpus: 16 classic algorithm tests (factorial, GCD, sorting, etc.)
- L2 challenge corpus: 8 concurrency tests (parallel join, channels, pipelines, mutex)
- Unit tests: bytecode, VM, GC, closures, compiler, verifier, type checker

## Next: v1

- Module system (import/export across graphs)
- Static type system with inference
- Drawing editor prototype
- Spec documents for language, bytecode, and VM

## Future

- Geometry-native semantics beyond v0 (crossings, winding, symmetry)
- Optimization passes (dead code, inlining, register allocation)
- Standard library
- Package format and distribution
- Generics / parametric polymorphism
