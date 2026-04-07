# uLua Documentation

uLua is a header-only C++20 binding library for Lua 5.1 and LuaJIT 2.x. It provides type-safe state management, zero-overhead stack manipulation, automatic userdata binding, and optional acceleration via LuaJIT internals.

Include the umbrella header to pull in everything:

```cpp
#include <ulua.hpp>
```

All public symbols live in the `ulua::` namespace.

## Getting Started

| Document | Purpose |
| --- | --- |
| [Getting Started](getting-started.md) | Prerequisites, installation, first program |

## Core API

| Document | Purpose |
| --- | --- |
| [State](state.md) | `ulua::state`, library loading, script execution, globals |
| [Stack and References](stack-and-references.md) | Stack slots, `stack_reference`, `registry_reference`, RAII |
| [Values and Conversions](values-and-conversions.md) | `type_traits<T>`, built-in conversions, `value_type` |
| [Tables](tables.md) | `ulua::table`, iteration, proxies, raw access, freezing |

## Advanced

| Document | Purpose |
| --- | --- |
| [Functions and Closures](functions-and-closures.md) | `ulua::function`, lambdas, `constant<>`, `overload<>` |
| [Userdata](userdata.md) | `user_traits<T>`, `member<>`, `property`, lifetimes |
| [Metatables](metatables.md) | Auto-generated metamethods, overrides, `meta::` slots |
| [Coroutines](coroutines.md) | `ulua::coroutine`, `resume`, yielding from C |
| [FFI](ffi.md) | LuaJIT `cdef`, cdata metatables, `UserCType` |

## Reference

| Document | Purpose |
| --- | --- |
| [Architecture](architecture.md) | Internal layout, accel path, `ULUA_JIT` / `ULUA_ACCEL` |
| [Error Handling](error-handling.md) | `ulua::error`, `function_result`, pcall semantics |
| [Cheatsheet](cheatsheet.md) | One-page snippet reference |
