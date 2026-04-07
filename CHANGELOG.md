# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-04-07

### Added
- CMake packaging with install/export support and `find_package(ulua)` integration.
- GitHub Actions CI for Ubuntu and macOS.
- Executable test coverage for smoke paths, advanced userdata/coroutine behavior, and doctest-based unit coverage for core modules.
- Example programs for basic use, greeting/global setup, and userdata binding.
- Root project hygiene files: `.clang-format`, `.editorconfig`, `.gitignore`, and contributor guidance.

### Fixed
- `stack::type_check<nil>` now uses the correct stack index in the non-accelerated path.
- `script_file(path, env)` now loads from disk instead of accidentally treating the path as inline source.
- `freeze_table` now blocks writes to existing keys as well as new keys.
- Readonly userdata descriptors now preserve their sentinel types correctly.
- LuaJIT detection now degrades cleanly when private headers are unavailable.

### Changed
- Public headers now live under `include/` instead of `includes/`.
- The LuaJIT internal-header opt-out path is exposed as `ULUA_NO_ACCEL`.
