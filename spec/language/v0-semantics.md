# Arcana Language Semantics — Version 0

## Values

| Type   | Tag        | Size   | Notes                          |
|--------|------------|--------|--------------------------------|
| Null   | VAL_NULL   | 0      | Singleton                      |
| Bool   | VAL_BOOL   | 1 bit  | true / false                   |
| Int    | VAL_I64    | 64 bit | Signed two's complement        |
| Float  | VAL_F64    | 64 bit | IEEE 754 double                |
| String | VAL_STRING | var    | Immutable, refcounted, UTF-8   |

## Truthiness

- `null` is falsy
- `false` is falsy
- `0` (int) is falsy
- `0.0` (float) is falsy
- `""` (empty string) is falsy
- Everything else is truthy

## Arithmetic

Binary operations `+`, `-`, `*`, `/`, `%` require both operands to be the
same numeric type (Int or Float). Division by zero is a runtime error.

Unary `-` (negation) applies to Int or Float.

## Comparison

`==` and `!=` compare any two values. Different-typed values are never equal
(except: Int and Float are not cross-compared in v0).

`<`, `<=`, `>`, `>=` require both operands to be the same numeric type.

## Logic

`not` converts its operand to a boolean via truthiness and negates it.

## Variables

`let` introduces a local binding. `assign` mutates an existing binding.
Variables are lexically scoped by region containment.

## Functions

Functions have a fixed arity. Parameters are positional, ordered by cyclic
port order on the function call node. Functions return a value (implicit
null if no explicit return).

## Control flow

`if` evaluates a condition; if truthy, executes the then region; otherwise
the else region. `while` repeatedly evaluates condition and body.

## Intrinsics

- `print(value)` — output a value to stdout
- `clock()` — return monotonic time in seconds (Float)

## Runtime errors

- Division by zero
- Stack overflow (>1024 operand values or >256 call frames)
- Type mismatch on arithmetic/comparison

Runtime errors halt execution and report the error with source provenance.

## Operand ordering

Operand order for binary operations and function arguments is determined
by the cyclic port order on the semantic graph node, not by textual
left-to-right convention. This is the geometry-native property of Arcana.

## Scoping

Region containment in the semantic graph defines lexical scope. A variable
defined in a region is visible to all nested regions but not to sibling
or parent regions.
