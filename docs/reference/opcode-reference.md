# Arcana Opcode Reference

All opcodes are defined via the `ARC_OPCODES` X-macro in `src/bytecode/opcodes.h`. This is the **single source of truth** — all consumers (VM, verifier, assembler, disassembler) derive from it.

## Encoding

Each instruction is 1 opcode byte followed by 0-4 operand bytes. Multi-byte operands are little-endian.

## Instruction Set

### Constants

| Opcode | Byte | Operands      | Stack Effect | Description           |
|--------|------|---------------|-------------|------------------------|
| CONST  | 0x01 | u16 const_idx | → val       | Push constant from pool|

### Stack

| Opcode | Byte | Operands | Stack Effect | Description           |
|--------|------|----------|-------------|------------------------|
| POP    | 0x02 | —        | val →       | Discard top of stack   |
| DUP    | 0x03 | —        | val → val val | Duplicate top        |

### Locals

| Opcode      | Byte | Operands  | Stack Effect | Description            |
|-------------|------|-----------|-------------|-------------------------|
| LOAD_LOCAL  | 0x10 | u16 slot  | → val       | Push local variable     |
| STORE_LOCAL | 0x11 | u16 slot  | val →       | Pop into local slot     |

### Globals (reserved)

| Opcode       | Byte | Operands | Stack Effect | Description            |
|--------------|------|----------|-------------|-------------------------|
| LOAD_GLOBAL  | 0x12 | u16 idx  | → val       | Push global variable    |
| STORE_GLOBAL | 0x13 | u16 idx  | val →       | Pop into global slot    |

### Arithmetic

| Opcode | Byte | Operands | Stack Effect  | Description             |
|--------|------|----------|--------------|--------------------------|
| ADD    | 0x20 | —        | a b → result | a + b (i64 or f64)      |
| SUB    | 0x21 | —        | a b → result | a - b                   |
| MUL    | 0x22 | —        | a b → result | a * b                   |
| DIV    | 0x23 | —        | a b → result | a / b (error if b == 0) |
| MOD    | 0x24 | —        | a b → result | a % b (integers only)   |
| NEG    | 0x25 | —        | a → result   | -a                      |

### Comparison

| Opcode | Byte | Operands | Stack Effect  | Description    |
|--------|------|----------|--------------|-----------------|
| EQ     | 0x30 | —        | a b → bool   | a == b          |
| NEQ    | 0x31 | —        | a b → bool   | a != b          |
| LT     | 0x32 | —        | a b → bool   | a < b           |
| LE     | 0x33 | —        | a b → bool   | a <= b          |
| GT     | 0x34 | —        | a b → bool   | a > b           |
| GE     | 0x35 | —        | a b → bool   | a >= b          |

### Logic

| Opcode | Byte | Operands | Stack Effect | Description     |
|--------|------|----------|-------------|------------------|
| NOT    | 0x36 | —        | a → bool    | Logical negation |

### Branching

Offsets are relative to the byte **after** the operand bytes (i.e., the next instruction).

| Opcode         | Byte | Operands    | Stack Effect | Description                    |
|----------------|------|-------------|-------------|--------------------------------|
| JUMP           | 0x40 | i32 offset  | —           | Unconditional jump             |
| JUMP_IF_FALSE  | 0x41 | i32 offset  | cond →      | Jump if top is falsy           |
| JUMP_IF_TRUE   | 0x42 | i32 offset  | cond →      | Jump if top is truthy          |

### Functions

| Opcode | Byte | Operands                     | Stack Effect          | Description          |
|--------|------|------------------------------|-----------------------|----------------------|
| CALL   | 0x50 | u16 func_idx, u8 argc, u8 pad | args... → result     | Call function        |
| RETURN | 0x51 | —                            | result →              | Return from function |

### Intrinsics

| Opcode    | Byte | Operands                    | Stack Effect  | Description           |
|-----------|------|-----------------------------|---------------|-----------------------|
| INTRINSIC | 0xF0 | u16 id, u8 argc, u8 pad     | args... → ... | Call built-in         |

Well-known intrinsic IDs:
- `0` — **print**: Pops `argc` values, prints them space-separated with trailing newline
- `1` — **clock**: Pushes current process CPU time as f64 (seconds)

### Control

| Opcode | Byte | Operands | Stack Effect | Description       |
|--------|------|----------|-------------|-------------------|
| HALT   | 0xFF | —        | —           | Stop VM execution |

## Value Types

The VM uses tagged values with four types:

| Tag      | Description                    |
|----------|--------------------------------|
| VAL_NULL | Null / absent value            |
| VAL_BOOL | Boolean (true/false)           |
| VAL_I64  | 64-bit signed integer          |
| VAL_F64  | 64-bit IEEE 754 floating point |

## Truthiness

- `null` → false
- `false` → false
- `0` (i64) → false
- `0.0` (f64) → false
- Everything else → true
