# ADR-0002: Geometry-Native Source Model

**Status:** Accepted
**Date:** 2026-08-15

## Context

The two architecture DOCX documents describe a conventional compiler input as a "Semantic Program Model" or AST. The kickoff prompt refines this into a geometry-native Arcana Semantic Graph.

## Decision

1. The two DOCX documents remain baseline architecture for compiler pipeline, bytecode format, and VM design.
2. The canonical compiler input is the **Arcana Semantic Graph** — not a conventional textual AST.
3. Normalized topology (containment, directed edges, cyclic port order) is semantic input to the compiler.
4. Pixels, rendering, strokes, and screen coordinates remain outside the compiler boundary.
5. After topology has been lowered into HIR/MIR, the compiler may become conventional.
6. Textual graph fixtures and bytecode assembly are engineering interfaces, not canonical user source.

## Version-0 Geometry-Native Semantics

- **Region containment** establishes lexical scope.
- **Directed edges** establish value/data dependencies.
- **Cyclic/clockwise port order** establishes operand/argument order.

## Consequences

- The semantic graph module is separate from HIR.
- Hand-authored fixtures (not a text parser) are the primary test input during early development.
- No Python-like textual syntax will be created as a user-facing shortcut.
