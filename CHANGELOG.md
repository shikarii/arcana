# Changelog

All notable changes to Arcana are documented here.

## [Unreleased]

### Added
- Bytecode vocabulary (27 opcodes) and versioned .mgc binary format
- Stack-based VM with call frames, locals, globals, intrinsics
- Bytecode assembler, disassembler, and independent verifier
- Arcana Semantic Graph library with topology validation
- Text fixture parser (.graph format) for test authoring
- Compiler pipeline: semantic graph to bytecode
- Reference interpreter for differential testing
- Structured diagnostics with source provenance
- Explicit HIR and MIR intermediate representations
- Semantic analysis with symbol/scope resolution
- String values (refcounted, immutable)
- Region containment as lexical scope
- Directed edges as value dependencies
- Cyclic port order as operand ordering
- Functions, if/else, while loops, recursion
- Debug metadata mapping bytecode to source elements
- CLI tools: arcana-run, arcana-dis, arcana-asm, arcana-verify, arcana-compile
- CI on Linux, Windows, and macOS
- 46+ tests across all layers
