# Arcana VM Specification

## Machine state

| Component     | Type           | Size | Description                    |
|---------------|----------------|------|--------------------------------|
| ip            | uint32_t       | —    | Instruction pointer            |
| stack         | ArcValue[]     | 1024 | Operand stack                  |
| sp            | uint16_t       | —    | Stack pointer (next free slot) |
| frames        | ArcFrame[]     | 256  | Call frame stack               |
| fp            | uint16_t       | —    | Frame pointer                  |
| globals       | ArcValue[]     | 256  | Global variable storage        |
| global_count  | uint16_t       | —    | Number of globals in use       |
| halted        | bool           | —    | Execution stopped              |
| error         | ArcVmError     | —    | Last error (if any)            |
| output        | FILE*          | —    | Output stream for print        |
| trace         | bool           | —    | Trace mode enabled             |

## Call frames

Each frame records:
- `func_idx` — which function is executing
- `return_ip` — where to resume after return
- `base_slot` — operand stack base for this frame's locals

## Execution

1. VM loads bytecode image
2. Starts executing function 0 (main) from its code_offset
3. Dispatches instructions in a loop
4. Stops on HALT or unrecoverable error

## Runtime values

Tagged union: NULL, BOOL, I64, F64, STRING.

Strings are heap-allocated with reference counting. The VM does not own
a garbage collector in v0.

## Intrinsics

| ID | Name  | Args | Returns | Behavior                      |
|----|-------|------|---------|-------------------------------|
| 0  | print | 1    | void    | Print value to output stream  |
| 1  | clock | 0    | Float   | Monotonic time in seconds     |

## Error handling

Runtime errors produce an `ArcVmError` with:
- error code
- human-readable message
- instruction pointer at time of error
- function index

The VM does not have exceptions. Errors are terminal.

## Limits

- Operand stack: 1024 values
- Call frames: 256 deep
- Globals: 256 slots
- Constant pool: 65536 entries (uint16_t index)
- Functions: 65536 entries (uint16_t index)
