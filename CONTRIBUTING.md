# Contributing to ardirec

Thank you for helping build a trustworthy open disturbance-analysis workstation.

## Development priorities

Compatibility and correctness outrank feature count. A small parser edge-case fix with a regression fixture is a first-class contribution.

## Before opening a pull request

1. Build core and run tests.
2. Add or update tests for behavior changes.
3. Do not include COMTRADE records you are not allowed to redistribute.
4. Keep UI code out of the core parser/calculation layers.
5. Do not copy proprietary vendor source code, screenshots as application assets, icons or protected UI artwork.
6. Document algorithm references for new engineering calculations.

## Core test command

```bash
cmake -S . -B build -G Ninja -DARDIREC_BUILD_DESKTOP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

## Coding style

C++ uses the repository `.clang-format`. Prefer explicit types at interfaces, RAII, immutable data where practical and deterministic error handling. Heavy work must never be introduced on the Qt UI thread.

## Commit/PR scope

Keep changes reviewable. Parser, calculations and rendering work should include evidence or test fixtures sufficient to reproduce the change.
