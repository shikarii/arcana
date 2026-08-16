# ADR-0001: C as Implementation Language

**Status:** Accepted
**Date:** 2026-08-15

## Context

The architecture documents default to Rust. The project owner has chosen C instead.

## Decision

Arcana's compiler, VM, bytecode tooling, and semantic graph library are implemented in **C17** (ISO/IEC 9899:2018).

## Rationale

- Project owner preference.
- C gives direct control over memory layout, which matters for a VM with tagged values, a GC, and compact bytecode.
- CMake build system for cross-platform portability.
- The architecture is designed so the host language choice is reversible at module boundaries.

## Consequences

- No RAII or destructors; manual resource management everywhere.
- No generics; use macros or void* where polymorphism is needed.
- No pattern matching; use switch statements on tagged enums.
- Strong typing via typedef'd IDs and explicit tagged unions.
- Build with `-std=c17 -Wall -Wextra -Werror` in CI.
