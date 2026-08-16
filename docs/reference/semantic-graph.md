# Arcana Semantic Graph

## Overview

The semantic graph is the canonical compiler input — a topology-aware program representation derived from drawn magic circle diagrams. It replaces traditional ASTs with a graph model where:

- **Containment = lexical scope** (regions)
- **Directed edges = value dependencies**
- **Cyclic port order = operand/argument ordering**

## Core Types

### Regions

Regions represent scopes. Each region has a kind, an optional parent, and a list of member nodes.

| Kind            | Description                 |
|-----------------|-----------------------------|
| MODULE          | Top-level module scope      |
| FUNCTION        | Function body scope         |
| BLOCK           | Nested block/scope          |
| THEN            | Then branch of if           |
| ELSE            | Else branch of if           |
| LOOP_BODY       | While loop body             |

### Nodes

Nodes represent operations, literals, and bindings. Each node belongs to exactly one region.

#### Literals

| Kind        | Attribute       | Description          |
|-------------|-----------------|----------------------|
| CONST_INT   | `int_value`     | 64-bit integer       |
| CONST_FLOAT | `float_value`   | 64-bit float         |
| CONST_BOOL  | `bool_value`    | Boolean              |
| CONST_NULL  | —               | Null literal         |

#### Arithmetic & Comparison

| Kind | Ports          | Description     |
|------|----------------|-----------------|
| ADD  | lhs, rhs → out | Binary addition |
| SUB  | lhs, rhs → out | Binary subtract |
| MUL  | lhs, rhs → out | Binary multiply |
| DIV  | lhs, rhs → out | Binary divide   |
| MOD  | lhs, rhs → out | Binary modulo   |
| NEG  | value → out    | Unary negate    |
| EQ   | lhs, rhs → out | Equality        |
| NEQ  | lhs, rhs → out | Inequality      |
| LT   | lhs, rhs → out | Less than       |
| LE   | lhs, rhs → out | Less or equal   |
| GT   | lhs, rhs → out | Greater than    |
| GE   | lhs, rhs → out | Greater or equal|
| NOT  | value → out    | Logical not     |

#### Bindings

| Kind    | Attribute | Ports       | Description             |
|---------|-----------|-------------|-------------------------|
| LET     | `name`    | value →     | Local variable binding  |
| VAR_REF | `name`    | → out       | Read a binding          |
| ASSIGN  | `name`    | value →     | Write to a binding      |

#### Control Flow

| Kind     | Attribute                      | Ports   | Description        |
|----------|--------------------------------|---------|--------------------|
| IF       | `then_region`, `else_region`   | cond →  | Conditional branch |
| WHILE    | `body_region`                  | cond →  | Loop               |
| SEQUENCE | —                              | —       | Ordered statements |

#### Functions

| Kind      | Attribute                         | Ports        | Description         |
|-----------|-----------------------------------|--------------|---------------------|
| FUNC_DEF  | `name`, `arity`, `body_region`    | —            | Function definition |
| FUNC_CALL | `name`                            | args... → out| Function call       |
| PARAM     | `name`                            | —            | Function parameter  |
| RETURN    | —                                 | value →      | Return from function|

#### Program Output

| Kind         | Ports    | Description           |
|--------------|----------|-----------------------|
| ROOT_OUTPUT  | value →  | Program result        |
| PRINT        | value →  | Intrinsic print       |

### Ports

Ports are connection points on nodes. Each port has:
- `dir`: INPUT or OUTPUT
- `role`: semantic label (e.g., "lhs", "rhs", "cond", "value")

### Edges

Directed edges connect an output port to an input port, representing data flow.

### Cyclic Port Order

Binary and call nodes use a cyclic port order array to determine operand ordering. The first two input ports in cyclic order are the left and right operands.

## Validation

`arc_graph_validate()` checks:
- All region parents exist and form a proper tree
- All edge endpoints exist
- Edges go from output ports to input ports (not input→input or output→output)
- All port owners are valid nodes
- Nodes reference valid regions

## Example: `5 + 10`

```
Region(MODULE)
├── Node(CONST_INT, value=5)
│   └── Port(OUTPUT, "out") ──edge──┐
├── Node(CONST_INT, value=10)       │
│   └── Port(OUTPUT, "out") ──edge──┤
├── Node(ADD)                       │
│   ├── Port(INPUT, "lhs") ←───────┘
│   ├── Port(INPUT, "rhs") ←───────┘
│   └── Port(OUTPUT, "out") ──edge──┐
│   cyclic_order: [lhs, rhs, out]   │
└── Node(ROOT_OUTPUT)               │
    └── Port(INPUT, "value") ←──────┘
```

This compiles to: `CONST 5, CONST 10, ADD, DUP, INTRINSIC print 1, HALT`
