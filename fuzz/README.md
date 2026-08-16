# Fuzzing

Arcana includes fuzz targets for security-critical input handling.

## Targets

| Target | File | What it tests |
|--------|------|---------------|
| fuzz_bytecode | fuzz_bytecode.c | .mgc deserialization + verifier |
| fuzz_fixture | fuzz_fixture.c | .graph text fixture parser |

## Building (requires clang with libFuzzer)

```bash
clang -fsanitize=fuzzer,address -o fuzz_bytecode \
    fuzz/fuzz_bytecode.c src/bytecode/format.c \
    src/bytecode/disassembler.c src/verifier/verifier.c \
    -Isrc

clang -fsanitize=fuzzer,address -o fuzz_fixture \
    fuzz/fuzz_fixture.c src/semantic_graph/semantic_graph.c \
    src/semantic_graph/fixture_parser.c \
    -Isrc
```

## Running

```bash
mkdir -p corpus
./fuzz_bytecode corpus/ -max_len=4096
./fuzz_fixture corpus/ -max_len=4096
```

Crashes are security bugs. See SECURITY.md for reporting.
