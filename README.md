# uLua

Header-only C++20 bindings for Lua 5.1 and LuaJIT 2.x.

uLua wraps `lua_State*` in a type-safe C++ interface: automatic stack management, lambda-to-`lua_CFunction` lifting, compile-time userdata descriptors, coroutines, sandboxed environments, and an optional LuaJIT FFI layer. One header, no generated code, no runtime overhead beyond what Lua itself costs.

## Requirements

- C++20 compiler — GCC 11+, Clang 13+, or MSVC 19.30+
- CMake 3.16+
- Lua 5.1 **or** LuaJIT 2.x headers and library

## Installation

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    ulua
    GIT_REPOSITORY https://github.com/can1357/ulua.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(ulua)

target_link_libraries(app PRIVATE ulua::ulua)
```

### Vendored

```cmake
add_subdirectory(third_party/ulua)
target_link_libraries(app PRIVATE ulua::ulua)
```

### System install

```cmake
find_package(ulua CONFIG REQUIRED)
target_link_libraries(app PRIVATE ulua::ulua)
```

## Quick Start

```cpp
#include <ulua.hpp>
#include <iostream>

int main() {
    ulua::state s;
    s.open_libraries(ulua::lib::base, ulua::lib::string);

    // Expose a C++ lambda as a Lua global.
    s["greet"] = [](std::string name) {
        return "hello, " + name + "!";
    };

    auto r = s.script(R"( return greet("world") )");
    if (!r) {
        std::cerr << r.error() << '\n';
        return 1;
    }
    std::cout << r.as<std::string>() << '\n'; // hello, world!
}
```

## Userdata Binding

Specialize `ulua::user_traits<T>` to make a C++ type available as a first-class Lua object with fields, methods, and operator metamethods:

```cpp
struct Vec2 { double x, y; double length() const; };

namespace ulua {
template<> struct user_traits<Vec2> {
    static constexpr const char* name = "Vec2";
    static constexpr auto fields = std::make_tuple(
        member<&Vec2::x>("x"),
        member<&Vec2::y>("y"),
        member<&Vec2::length>("length")
    );
};
}

// Expose a constructor.
s["Vec2"] = [](double x, double y) { return Vec2{x, y}; };
```

From Lua:

```lua
local v = Vec2(3, 4)
print(v:length())  -- 5.0
```

## Feature Overview

| Feature | API |
| --- | --- |
| State management | `ulua::state`, `open_libraries`, `script`, `script_file`, `load` |
| Globals | `s["key"] = value` / `T v = s["key"]` |
| Tables | `ulua::table`, range-for iteration, `freeze_table`, raw access |
| Functions | `ulua::function`, `function_result`, multi-return via `as<tuple<…>>` |
| C++ in Lua | Lambdas, `constant<&fn>`, `overload<…>` for dispatch |
| Named arguments | `named<T, "name"_n>`, `named_opt<T, "name"_n>` |
| Userdata | `user_traits<T>`, `member<>`, `property`, auto metamethods |
| Coroutines | `ulua::coroutine`, `resume`, yield-from-C |
| Environments | `ulua::environment`, sandboxing, per-chunk `_ENV` |
| References | `stack_reference` (transient), `registry_reference` (long-lived) |
| LuaJIT FFI | `ulua::ffi::cdef`, `set_metatable`, `UserCType` (when `ULUA_JIT=1`) |
| Error handling | `ulua::error`, `arg_error`, `type_error`, `function_result::is_error` |

## Building and Testing

```sh
git clone https://github.com/can1357/ulua.git
cd ulua
cmake -B build -DULUA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### CMake Options

| Option | Default | Purpose |
| --- | --- | --- |
| `ULUA_BUILD_TESTS` | `ON` | Build the test suite |
| `ULUA_BUILD_EXAMPLES` | `ON` | Build example programs |
| `ULUA_INSTALL` | `ON` | Install headers and CMake package files |
| `ULUA_SUPPORTED_LUA_PROVIDER` | auto | Force `LuaJIT` or `Lua51`; auto-detects by default |
| `ULUA_NO_ACCEL` | `OFF` | Disable LuaJIT internal-header acceleration |

## Documentation

Full documentation lives in [`docs/`](docs/index.md):

- [Getting Started](docs/getting-started.md)
- [State](docs/state.md)
- [Values and Conversions](docs/values-and-conversions.md)
- [Tables](docs/tables.md)
- [Functions and Closures](docs/functions-and-closures.md)
- [Userdata](docs/userdata.md)
- [Coroutines](docs/coroutines.md)
- [Error Handling](docs/error-handling.md)
- [FFI](docs/ffi.md)
- [Cheatsheet](docs/cheatsheet.md)

## License

MIT — see [LICENSE](LICENSE).
