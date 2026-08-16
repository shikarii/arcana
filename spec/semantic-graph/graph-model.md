# Arcana Semantic Graph Specification

## Overview

The Arcana Semantic Graph is the canonical compiler input. It represents
the normalized topology of a drawn magic-circle program.

## Elements

### Nodes

Every operation, literal, binding, or control-flow construct is a node.

| Kind          | Ports              | Semantics                    |
|---------------|--------------------|------------------------------|
| CONST_INT     | out                | Integer literal              |
| CONST_FLOAT   | out                | Float literal                |
| CONST_BOOL    | out                | Boolean literal              |
| CONST_NULL    | out                | Null literal                 |
| ADD..MOD      | lhs, rhs, out      | Binary arithmetic            |
| NEG           | value/in, out      | Unary negation               |
| NOT           | value/in, out      | Logical negation             |
| EQ..GE        | lhs, rhs, out      | Comparison                   |
| LET           | value              | Local binding (name in attr) |
| VAR_REF       | out                | Variable reference           |
| ASSIGN        | value              | Variable mutation            |
| IF            | cond               | Conditional (then/else regions) |
| WHILE         | cond               | Loop (body region)           |
| FUNC_DEF      | —                  | Function (body region, arity)|
| FUNC_CALL     | arg0..argN, out    | Function call                |
| PARAM         | —                  | Function parameter           |
| RETURN        | value              | Return from function         |
| PRINT         | value/in           | Intrinsic output             |
| ROOT_OUTPUT   | value/in           | Program result               |
| SEQUENCE      | —                  | Statement ordering           |

### Ports

Connection points on nodes. Each port has:
- **id** — unique within the graph
- **owner** — the node this port belongs to
- **direction** — INPUT, OUTPUT, or BIDIRECTIONAL
- **role** — semantic name (e.g., "lhs", "rhs", "out", "cond", "value")

### Edges

Directed connections from an output port to an input port. Edges represent
value/data dependencies.

### Regions

Containment boundaries that define scope:
- **MODULE** — top-level program scope
- **FUNCTION** — function body scope
- **BLOCK** — nested block scope
- **THEN** / **ELSE** — conditional branches
- **LOOP_BODY** — loop body scope

Regions nest. Every node belongs to exactly one region.

### Cyclic Port Order

The order of ports around a node is semantically significant. For binary
operations, cyclic port order determines which operand is first (lhs) and
second (rhs). For function calls, it determines argument order.

## Validation rules

1. All IDs are unique within their type (node, port, edge, region)
2. Every node belongs to exactly one valid region
3. Region parentage is acyclic
4. Every edge endpoint references a valid port
5. A port's owner must exist
6. Cyclic port order contains each port exactly once
7. Input ports on known node kinds must be connected
8. Source provenance IDs are preserved through lowering
