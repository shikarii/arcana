# Arcana Testing Strategy

## Test layers

| Layer | What it tests | Location |
|-------|---------------|----------|
| Unit | Individual functions, encoders, decoders | tests/test_*.c (per-module) |
| Integration | Full pipeline (graph → bytecode → VM → result) | tests/test_pipeline.c, test_pipeline_e2e.c |
| Compile-fail | Invalid programs produce correct diagnostics | tests/test_pipeline.c |
| Error recovery | Multiple errors reported, poison propagation | tests/test_error_recovery.c |
| Golden | Deterministic disassembly output | tests/test_pipeline.c |
| Differential | Reference interpreter vs compiler+VM | tests/test_infra.c |
| Property | Encode/decode roundtrips with random data | tests/test_bytecode.c |

## Test files (10 per-module files)

| File | Tests | Covers |
|------|-------|--------|
| test_bytecode.c | Opcode metadata, const pool, serialization, roundtrips | src/bytecode/ |
| test_vm.c | Arithmetic, comparisons, branches, locals, globals, strings, bitwise, casts | src/vm/ |
| test_vm_collections.c | Arrays, maps, exceptions, intrinsics, closures | src/vm/ (collections + closures) |
| test_gc.c | GC collect/preserve, array/map tracing, stress mode | src/runtime/gc.c |
| test_graph.c | Semantic graph, HIR, MIR construction and validation, semantic lowering | src/semantic_graph/, src/hir/, src/mir/, src/semantic/ |
| test_pipeline.c | End-to-end compilation, golden disassembly, short-circuit, compile-fail | src/compiler/ + pipeline |
| test_pipeline_e2e.c | Function calls, if/else, fibonacci, while loops, NOT | Full pipeline e2e |
| test_infra.c | Platform abstraction, diagnostics, string values, reference interpreter | src/platform/, diagnostics, interpreter |
| test_runtime.c | Arena allocator, type checker, records, diagnostic codes | src/common/, src/typecheck/, src/runtime/ |
| test_verifier.c | Valid/invalid bytecode, stack consistency, bad jumps | src/verifier/ |
| test_error_recovery.c | Multi-error, poison propagation, HIR_POISON dump/validation | Error recovery system |

Driver: `test_all.c` — declares externs, sub-runner functions per module, calls all from main().

## Running tests

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build -C Debug -V
```

CTest runs two tests:
1. `arcana_tests` — all 123 unit/integration tests
2. `check_limits` — LOC enforcement (600 lines/file, 60 lines/function)

## LOC enforcement

`tools/check-limits.py` enforces:
- Maximum 600 lines per .c/.h file
- Maximum 60 lines per function
- Scans `src/` and `tests/` directories
- Integrated as CTest test (`check_limits`)

## Writing tests

Tests use a minimal custom assertion framework in `tests/test_harness.h`. Each test is a `void test_name(void)` function.

To add a test:
1. Write the function in the appropriate per-module test file
2. Add `extern void test_name(void);` in test_all.c
3. Add `RUN(test_name);` in the appropriate sub-runner function

## Error recovery testing

The error recovery system uses HIR_POISON nodes. When semantic analysis encounters an error (undefined variable, arity mismatch, etc.), it returns a poison node instead of NULL and continues analyzing. This reports multiple diagnostics per compilation.

Tests verify:
- Multiple errors in one program produce multiple diagnostics
- Poison propagation prevents cascade errors (1 root error → 1 diagnostic, not N)
- Poison nodes dump correctly and pass validation

## Differential testing

The reference interpreter (`src/interpreter/`) directly evaluates semantic graphs. For any pure program, compare:

```
arc_interpret(graph).result == arc_vm_result(vm)
```

This catches lowering bugs without making the interpreter the production runtime. Currently 3 differential tests: 5+10, function call, NOT operator.

## Property testing

`test_property_roundtrip_random` generates diverse constant values (INT64_MAX, INT64_MIN, -0.0, empty string, etc.) and verifies they survive a full serialization roundtrip.

## Compile-fail testing

`test_compile_fail_diagnostic` verifies that malformed graphs produce structured diagnostics with correct error codes rather than crashes.
