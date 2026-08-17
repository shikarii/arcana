# Arcana Opcode Reference

All opcodes are defined via the `ARC_OPCODES` X-macro in `src/bytecode/opcodes.h`. This is the **single source of truth** — all consumers (VM, verifier, assembler, disassembler) derive from it.

## Encoding

Each instruction is 1 opcode byte followed by 0-4 operand bytes. Multi-byte operands are little-endian.

## Instruction Set (41 opcodes)

### Constants

| Opcode | Byte | Operands      | Stack Effect | Description           |
|--------|------|---------------|-------------|------------------------|
| CONST  | 0x01 | u16 const_idx | → val       | Push constant from pool|

### Stack

| Opcode | Byte | Operands | Stack Effect    | Description      |
|--------|------|----------|----------------|-------------------|
| POP    | 0x02 | —        | val →          | Discard top       |
| DUP    | 0x03 | —        | val → val val  | Duplicate top     |

### Locals & Globals

| Opcode       | Byte | Operands  | Stack Effect | Description        |
|--------------|------|-----------|-------------|--------------------|
| LOAD_LOCAL   | 0x10 | u16 slot  | → val       | Push local         |
| STORE_LOCAL  | 0x11 | u16 slot  | val →       | Pop into local     |
| LOAD_GLOBAL  | 0x12 | u16 idx   | → val       | Push global        |
| STORE_GLOBAL | 0x13 | u16 idx   | val →       | Pop into global    |

### Type Conversion

| Opcode   | Byte | Operands | Stack Effect | Description          |
|----------|------|----------|-------------|----------------------|
| CAST_I64 | 0x14 | —        | val → i64   | bool→0/1, f64→trunc, str→parse, null→0 |
| CAST_F64 | 0x15 | —        | val → f64   | bool→0.0/1.0, i64→convert, str→parse, null→0.0 |
| CAST_STR | 0x16 | —        | val → str   | any→formatted string |

### Arithmetic

| Opcode | Byte | Operands | Stack Effect  | Description                        |
|--------|------|----------|--------------|-------------------------------------|
| ADD    | 0x20 | —        | a b → result | a + b (i64, f64, or string concat) |
| SUB    | 0x21 | —        | a b → result | a - b                              |
| MUL    | 0x22 | —        | a b → result | a * b                              |
| DIV    | 0x23 | —        | a b → result | a / b (error if b == 0)            |
| MOD    | 0x24 | —        | a b → result | a % b (integers only)              |
| NEG    | 0x25 | —        | a → result   | -a                                 |

### String Operations

| Opcode    | Byte | Operands | Stack Effect          | Description              |
|-----------|------|----------|----------------------|--------------------------|
| STR_LEN   | 0x26 | —        | str → i64            | Length of string          |
| STR_SLICE | 0x27 | —        | str start end → str  | Substring [start, end)   |
| STR_INDEX | 0x28 | —        | str idx → str        | Single character at index|

### Bitwise (i64 only)

| Opcode  | Byte | Operands | Stack Effect  | Description |
|---------|------|----------|--------------|-------------|
| BIT_AND | 0x29 | —        | a b → result | a & b       |
| BIT_OR  | 0x2A | —        | a b → result | a \| b      |
| BIT_XOR | 0x2B | —        | a b → result | a ^ b       |
| BIT_NOT | 0x2C | —        | a → result   | ~a          |
| SHL     | 0x2D | —        | a b → result | a << b      |
| SHR     | 0x2E | —        | a b → result | a >> b      |

### Comparison

| Opcode | Byte | Operands | Stack Effect | Description |
|--------|------|----------|-------------|-------------|
| EQ     | 0x30 | —        | a b → bool  | a == b      |
| NEQ    | 0x31 | —        | a b → bool  | a != b      |
| LT     | 0x32 | —        | a b → bool  | a < b       |
| LE     | 0x33 | —        | a b → bool  | a <= b      |
| GT     | 0x34 | —        | a b → bool  | a > b       |
| GE     | 0x35 | —        | a b → bool  | a >= b      |

### Logic

| Opcode | Byte | Operands | Stack Effect | Description      |
|--------|------|----------|-------------|-------------------|
| NOT    | 0x36 | —        | a → bool    | Logical negation  |

### Branching

Offsets are relative to the byte **after** the operand bytes.

| Opcode        | Byte | Operands   | Stack Effect | Description           |
|---------------|------|------------|-------------|-----------------------|
| JUMP          | 0x40 | i32 offset | —           | Unconditional jump    |
| JUMP_IF_FALSE | 0x41 | i32 offset | cond →      | Jump if top is falsy  |
| JUMP_IF_TRUE  | 0x42 | i32 offset | cond →      | Jump if top is truthy |

### Exception Handling

| Opcode    | Byte | Operands      | Stack Effect | Description                    |
|-----------|------|---------------|-------------|--------------------------------|
| TRY_BEGIN | 0x43 | i32 catch_off | —           | Push handler (catch IP + state)|
| TRY_END   | 0x44 | —             | —           | Pop handler                    |
| THROW     | 0x45 | —             | val →       | Unwind to nearest handler      |

`TRY_BEGIN` pushes `{ catch_ip, stack_height, frame_depth }`. `THROW` unwinds frames to handler's frame_depth, restores stack, pushes thrown value, jumps to catch_ip. No handler → runtime error.

### Functions

| Opcode | Byte | Operands                      | Stack Effect     | Description    |
|--------|------|-------------------------------|------------------|----------------|
| CALL   | 0x50 | u16 func_idx, u8 argc, u8 pad | args... → result | Call function  |
| RETURN | 0x51 | —                             | result →         | Return         |

### Closures

| Opcode        | Byte | Operands      | Stack Effect | Description                  |
|---------------|------|---------------|-------------|------------------------------|
| CLOSURE       | 0x52 | u16 func_idx  | → closure   | Create closure from func_idx |
| GET_UPVALUE   | 0x53 | u16 upval_idx | → val       | Read upvalue                 |
| SET_UPVALUE   | 0x54 | u16 upval_idx | val →       | Write upvalue                |
| CLOSE_UPVALUE | 0x55 | —             | —           | Close upvalue at stack top   |

Lua-style upvalues: open upvalues point to stack slots; closed on scope exit to heap values.

### Collections

| Opcode    | Byte | Operands       | Stack Effect           | Description                    |
|-----------|------|----------------|------------------------|--------------------------------|
| ARRAY_NEW | 0x60 | u16 count      | val×count → array      | Create array from stack values |
| INDEX_GET | 0x61 | —              | container key → val    | Get element                    |
| INDEX_SET | 0x62 | —              | container key val →    | Set element                    |
| LENGTH    | 0x63 | —              | container → i64        | Length (string/array/map)      |
| MAP_NEW   | 0x64 | u16 pair_count | (key val)×count → map  | Create map from pairs          |

### Intrinsics

| Opcode    | Byte | Operands                   | Stack Effect  | Description |
|-----------|------|----------------------------|---------------|-------------|
| INTRINSIC | 0xF0 | u16 id, u8 argc, u8 pad    | args... → ... | Call built-in|

| ID | Name     | Signature            | Description                            |
|----|----------|----------------------|----------------------------------------|
| 0  | print    | (val...) → null      | Print values space-separated + newline |
| 1  | clock    | () → f64             | Process CPU time in seconds            |
| 2  | type     | (val) → string       | Type name string                       |
| 3  | assert   | (cond) → null        | Runtime error if falsy                 |
| 4  | tostring | (val) → string       | String representation                  |
| 5  | input    | () → string          | Read line from stdin                   |
| 6  | len      | (container) → i64    | Length of string/array/map             |
| 7  | push     | (array, val) → null  | Append to array                        |
| 8  | keys     | (map) → array        | Array of map keys                      |

### Control

| Opcode | Byte | Operands | Stack Effect | Description       |
|--------|------|----------|-------------|-------------------|
| HALT   | 0xFF | —        | —           | Stop VM execution |

## Value Types

| Tag      | Description                    |
|----------|--------------------------------|
| VAL_NULL | Null / absent value            |
| VAL_BOOL | Boolean (true/false)           |
| VAL_I64  | 64-bit signed integer          |
| VAL_F64  | 64-bit IEEE 754 floating point |
| VAL_OBJ  | Heap object pointer            |

## Heap Object Types

| Type        | Description                              |
|-------------|------------------------------------------|
| OBJ_STRING  | Immutable UTF-8 string (hash cached)     |
| OBJ_ARRAY   | Dynamic array (items + count + capacity) |
| OBJ_MAP     | Hash map (keys + values + count + cap)   |
| OBJ_CLOSURE | Function + captured upvalues             |
| OBJ_UPVALUE | Mutable reference to captured variable   |
| OBJ_RECORD  | Named field record (struct-like)         |

## Truthiness

- `null` → false
- `false` → false
- `0` (i64) → false
- `0.0` (f64) → false
- Everything else → true (including empty string, empty array, empty map)
