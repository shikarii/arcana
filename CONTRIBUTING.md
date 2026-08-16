# Contributing to Arcana

## Getting started

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build -V
```

## Development rules

- **Deterministic compilation.** Same graph must always produce identical
  bytecode. No hash-map iteration order or timestamps in output.
- **Layer separation.** The VM does not import compiler internals; the compiler
  does not import VM internals.
- **Single source of truth.** Opcodes defined once in `src/bytecode/opcodes.h`;
  consumers derive.
- **Stable element IDs.** Every semantic element retains identity through the
  pipeline into debug metadata.
- **No textual user syntax.** Text forms exist only for assembly, fixtures,
  debug dumps, and serialization.
- **Tests required.** No compiler/runtime feature is complete without tests.

## Pull requests

Use feature branches and PRs against `main`. A PR should explain what changed,
why, and what tests were added.

Large semantic changes should link an ADR in `docs/adr/`.

## Architecture

See `docs/internals/` for subsystem documentation. The compiler pipeline is:

```
Semantic Graph → Semantic Analysis → HIR → MIR → Bytecode → Verifier → VM
```

Each layer has its own module under `src/`. See the README for the full layout.

## Reporting issues

Use the GitHub issue tracker. Include:

- what you expected,
- what happened,
- a minimal reproduction (semantic graph fixture or bytecode if applicable).
