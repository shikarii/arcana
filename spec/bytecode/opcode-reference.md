# Arcana Opcode Reference

All opcodes are defined in `src/bytecode/opcodes.h` (single source of truth).

## Notation

```
OPCODE operands    ; stack_before -> stack_after
```

## Constants

```
CONST  u16 idx     ; ... -> ..., value     Push constant pool[idx]
```

## Stack

```
POP                ; ..., a -> ...          Discard top
DUP                ; ..., a -> ..., a, a   Duplicate top
```

## Local Variables

```
LOAD_LOCAL  u16 slot  ; ... -> ..., value   Push local[slot]
STORE_LOCAL u16 slot  ; ..., value -> ...   Pop into local[slot]
```

## Global Variables

```
LOAD_GLOBAL  u16 idx  ; ... -> ..., value  Push global[idx]
STORE_GLOBAL u16 idx  ; ..., value -> ...  Pop into global[idx]
```

## Arithmetic

```
ADD            ; ..., a, b -> ..., result   a + b (Int or Float)
SUB            ; ..., a, b -> ..., result   a - b
MUL            ; ..., a, b -> ..., result   a * b
DIV            ; ..., a, b -> ..., result   a / b (error if b == 0)
MOD            ; ..., a, b -> ..., result   a % b (error if b == 0)
NEG            ; ..., a -> ..., result      -a
```

## Comparison

```
EQ             ; ..., a, b -> ..., bool     a == b
NEQ            ; ..., a, b -> ..., bool     a != b
LT             ; ..., a, b -> ..., bool     a < b
LE             ; ..., a, b -> ..., bool     a <= b
GT             ; ..., a, b -> ..., bool     a > b
GE             ; ..., a, b -> ..., bool     a >= b
```

## Logic

```
NOT            ; ..., a -> ..., bool        !truthy(a)
```

## Branching

Offsets are relative to the end of the operand (signed i32).

```
JUMP           i32 offset   ; ... -> ...             Unconditional jump
JUMP_IF_FALSE  i32 offset   ; ..., cond -> ...       Jump if falsy
JUMP_IF_TRUE   i32 offset   ; ..., cond -> ...       Jump if truthy
```

## Functions

```
CALL    u16 func_idx, u8 argc, u8 pad
        ; ..., arg0, ..., argN -> ..., result
        Pop argc args, call function, push return value.

RETURN  ; ..., value -> (caller) ..., value
        Pop return value, restore caller frame, push value.
```

## Intrinsics

```
INTRINSIC  u16 id, u8 argc, u8 pad
           ; ..., arg0, ..., argN -> ..., [result]
           Built-in operations. ID 0 = print, ID 1 = clock.
```

## Control

```
HALT       ; ... -> (stopped)   Terminate execution.
```

## Encoding

Each instruction is 1 byte opcode + N operand bytes (see operand column).
Multi-byte operands are little-endian. Opcode bytes are stable across
versions — see `opcodes.h` for canonical assignments.
