# Arcana Bytecode Format (.mgc)

## Overview

The `.mgc` file format is the serialized representation of an `ArcBytecodeImage`. All multi-byte values are **little-endian**.

## File Layout

```
┌──────────────────────────────┐
│  Header           (12 bytes) │
├──────────────────────────────┤
│  Constant Pool    (variable) │
├──────────────────────────────┤
│  Function Table   (variable) │
├──────────────────────────────┤
│  Code Section     (variable) │
├──────────────────────────────┤
│  Debug Section    (optional) │
└──────────────────────────────┘
```

## Header (12 bytes)

| Offset | Size | Field          | Description                    |
|--------|------|----------------|--------------------------------|
| 0      | 4    | magic          | `0x41 0x52 0x43 0x41` ("ARCA") |
| 4      | 1    | version_major  | Currently `0`                  |
| 5      | 1    | version_minor  | Currently `1`                  |
| 6      | 2    | flags          | Reserved (0)                   |
| 8      | 2    | const_count    | Number of constant pool entries|
| 10     | 2    | func_count     | Number of function records     |

## Constant Pool

Each entry starts with a 1-byte tag:

| Tag | Type   | Payload                           |
|-----|--------|-----------------------------------|
| 0   | null   | (none)                            |
| 1   | bool   | 1 byte (0=false, 1=true)          |
| 2   | i64    | 8 bytes, little-endian signed     |
| 3   | f64    | 8 bytes, IEEE 754 double          |
| 4   | string | u32 length + raw UTF-8 bytes      |

## Function Table

Each function record (15 bytes):

| Offset | Size | Field            | Description                      |
|--------|------|------------------|----------------------------------|
| 0      | 2    | name_const_idx   | Index into constant pool (string)|
| 2      | 1    | arity            | Parameter count                  |
| 3      | 2    | local_count      | Total local variable slots       |
| 5      | 2    | max_stack        | Maximum operand stack depth      |
| 7      | 4    | code_offset      | Byte offset into code section    |
| 11     | 4    | code_length      | Byte count of function code      |

Function index 0 is always `main` (the entry point).

## Code Section

Prefixed with a u32 byte count, followed by raw bytecode bytes. Instructions are encoded as:

```
[opcode: 1 byte] [operands: 0-4 bytes]
```

See [opcode-reference.md](opcode-reference.md) for the full instruction set.

## Debug Section (optional)

Only present if data remains after the code section.

- u32: entry count
- Per entry (18 bytes):
  - u16: func_idx
  - u32: bc_start (bytecode offset)
  - u32: bc_end (bytecode offset, exclusive)
  - u64: element_id (source element identity)

Debug entries map bytecode ranges back to source drawing elements via their stable `element_id`.
