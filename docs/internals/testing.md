# Arcana Testing Strategy

## Test layers

| Layer | What it tests | Location |
|-------|---------------|----------|
| Unit | Individual functions, encoders, decoders | tests/unit/ |
| Integration | Full pipeline (graph → bytecode → VM → result) | tests/integration/ |
| Compile-fail | Invalid programs produce correct diagnostics | tests/compile-fail/ |
| Golden | Deterministic IR/disassembly output | tests/golden/ |
| Differential | Reference interpreter vs compiler+VM | tests/ |
| Property | Encode/decode roundtrips with random data | tests/ |

## Running tests

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build -V
```

## Writing tests

Tests use a minimal custom assertion framework in `tests/test_all.c`.
Each test is a `void test_name(void)` function registered in `main()`.

To add a test:
1. Write the function
2. Add `RUN(test_name);` in the appropriate section of `main()`

## Differential testing

The reference interpreter (`src/interpreter/`) directly evaluates semantic
graphs. For any pure program, compare:

```
arc_interpret(graph).result == arc_vm_result(vm)
```

This catches lowering bugs without making the interpreter the production
runtime.

## Property testing

`test_property_roundtrip_random` generates diverse constant values
(INT64_MAX, INT64_MIN, -0.0, empty string, etc.) and verifies they
survive a full serialization roundtrip.

## Compile-fail testing

`test_compile_fail_diagnostic` verifies that malformed graphs produce
structured diagnostics with correct error codes rather than crashes.
