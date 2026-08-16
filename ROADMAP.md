# Arcana Roadmap

## Current: v0 prototype

The v0 prototype proves the geometry-native design by compiling topology-aware
semantic graphs through a full pipeline to a stack-based VM.

### Complete
- Bytecode format, assembler, disassembler, verifier
- Stack VM with tagged values and call frames
- Semantic graph with regions, edges, cyclic port order
- Compiler pipeline (graph → HIR → MIR → bytecode)
- Functions, branching, loops, recursion
- String values, global variables
- Reference interpreter for differential testing
- Multi-platform CI

### In progress
- Explicit HIR/MIR with inspection and golden tests
- Structured diagnostics with stable error codes
- Spec documents for language, bytecode, and VM
- Fuzzing infrastructure

## Next: v0.2

- Type system for primitive operations
- Closures and variable capture
- Arrays and records
- Mark-sweep garbage collector
- Module system

## Future

- Drawing editor prototype
- Geometry-native semantics beyond v0 (crossings, winding, symmetry)
- Optimization passes
- Richer runtime objects
- Standard library
- Package format
