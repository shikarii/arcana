# Arcana Bytecode Format (.mgc)

## File structure

```
[Header]
[Constant Pool]
[Function Table]
[Code Section]
[Debug Section (optional)]
```

## Header

| Offset | Size | Content              |
|--------|------|----------------------|
| 0      | 4    | Magic: "ARCA" (0x41 0x52 0x43 0x41) |
| 4      | 1    | Major version        |
| 5      | 1    | Minor version        |

Current version: 0.1

## Constant Pool

Preceded by a `uint32_t` entry count.

Each entry:
| Field | Size | Description           |
|-------|------|-----------------------|
| tag   | 1    | 0=null, 1=bool, 2=i64, 3=f64, 4=string |
| data  | var  | Tag-dependent payload |

- null: no payload
- bool: 1 byte (0 or 1)
- i64: 8 bytes, little-endian
- f64: 8 bytes, IEEE 754 LE
- string: uint32_t length + length bytes UTF-8

Constants are deduplicated: identical values share the same pool index.

## Function Table

Preceded by a `uint16_t` function count.

Each function record:
| Field          | Size | Description                    |
|----------------|------|--------------------------------|
| name_const_idx | 2    | Index into constant pool (string) |
| arity          | 1    | Number of parameters           |
| local_count    | 2    | Total local variable slots     |
| max_stack      | 2    | Maximum operand stack depth    |
| code_offset    | 4    | Byte offset into code section  |
| code_length    | 4    | Byte length of function code   |

Function 0 is always `main` (the program entry point).

## Code Section

Preceded by a `uint32_t` code length.

Contains a sequence of instructions. Each instruction is 1 opcode byte
followed by 0-4 operand bytes.

See `spec/bytecode/opcode-reference.md` for the full instruction set.

## Debug Section

Preceded by a `uint32_t` entry count.

Each debug entry maps a bytecode range to a source element:
| Field      | Size | Description                |
|------------|------|----------------------------|
| func_idx   | 2    | Which function             |
| bc_start   | 4    | Bytecode start offset      |
| bc_end     | 4    | Bytecode end offset        |
| element_id | 8    | Stable source element ID   |

## Endianness

All multi-byte integers are little-endian.

## Compatibility

Readers MUST check the magic bytes and version. Unknown versions MUST be
rejected with a clear error, not silently misinterpreted.
