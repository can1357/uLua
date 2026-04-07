# Contributing to uLua

## Prerequisites

- A C++20-capable compiler (GCC 13+, Clang 17+, Apple Clang 15+, MSVC 2022+).
- CMake 3.16 or newer.
- LuaJIT 2.x (for running tests).

## Building and Testing

If you have [`just`](https://github.com/casey/just) installed, the canonical workflow is a single command:

```bash
just          # list recipes
just verify   # configure + build + test
```

Other useful recipes: `just configure`, `just build`, `just test`, `just examples`, `just install`, `just fmt`, `just fmt-check`, `just clean`.

Without `just`, the equivalent CMake invocations are:

```bash
cmake -S . -B build -DULUA_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build
```

## Code Style

- Follow the `.clang-format` configuration in the repo root.
- Spaces inside parentheses: `foo( x, y )`.
- 4-space indentation, no tabs.
- Allman brace style for namespaces, classes, and functions.

## Commit Messages

- Use imperative mood: "Fix bug" not "Fixed bug" or "Fixes bug".
- Keep the summary line brief (under 72 characters).
- Add detail in the body when the change is non-obvious.

## Pull Request Process

1. Fork the repository and create a feature branch from `main`.
2. Make your changes and ensure all tests pass.
3. Open a pull request against `main` with a clear description of the change.
