# Contributing to ArDiRec

Thank you for contributing to an open-source COMTRADE workstation for protection and disturbance engineering.

## Before you begin

- Search [existing issues](https://github.com/masarray/ardirec/issues) and open a focused issue for a substantial change.
- Prefer one engineering problem per pull request; include expected/actual behavior and measurable acceptance criteria.
- Do not include proprietary source, artwork, user recordings or field datasets without redistribution permission.
- Security vulnerabilities should follow [responsible disclosure](SECURITY.md), not a public issue.

## Build and test

Install CMake 3.24+ and a C++20 compiler. For desktop development, install Qt 6.8+ with Qt Quick/QML.

```bash
cmake -S . -B build -DARDIREC_BUILD_DESKTOP=ON -DARDIREC_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

To build and test the standalone core without Qt:

```bash
cmake -S . -B build-core -DARDIREC_BUILD_DESKTOP=OFF -DARDIREC_BUILD_TESTS=ON
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

## Engineering standards

1. Preserve source samples, units, timestamps, phase interpretation and numerical meaning; never hide unsupported behavior behind fabricated values.
2. Add deterministic regression coverage for changes to parsers, calculations or synchronization.
3. Keep the UI thread responsive. Avoid record-length traversals or heavy calculations inside Qt render callbacks.
4. Honor the [performance contract](docs/PERFORMANCE_CONTRACT.md), including bounded memory and safe large-record access.
5. Separate portable core logic from Qt presentation; document assumptions and formula/window conventions.
6. Test and document any change to install, launch or uninstall behavior on supported platforms.

C++ formatting follows [`.clang-format`](.clang-format). Favor small, testable interfaces, safe lifetimes and explicit failure handling.

## Pull request checklist

- Explain **why** the change matters and which user workflow it improves.
- Include tests or a reproducible fixture, plus the exact commands used.
- For GUI changes, attach real screenshots or recordings when possible; avoid mock screenshots presented as working software.
- For calculation changes, state units, scaling, timing window and reference values.
- Verify that no confidential data or third-party assets were added.

The repository is licensed under [GPL-3.0-or-later](LICENSE).
