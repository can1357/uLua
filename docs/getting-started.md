# Getting Started

This guide takes you from a bare project to running a C++ callback from Lua in a few minutes.

## Prerequisites

- A C++20 compiler (GCC 11+, Clang 13+, MSVC 19.30+).
- CMake 3.16 or newer.
- Lua 5.1 **or** LuaJIT 2.x headers and library available to your build.

uLua itself is header-only. It has no runtime dependency other than Lua.

## Installation

### Option 1: FetchContent (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(
    ulua
    GIT_REPOSITORY https://github.com/can1357/ulua.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(ulua)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE ulua::ulua luajit)
```

### Option 2: add_subdirectory

Vendor the repo into `third_party/ulua/` and:

```cmake
add_subdirectory(third_party/ulua)
target_link_libraries(app PRIVATE ulua::ulua)
```

### Option 3: find_package

After `cmake --install`, consume the exported package:

```cmake
find_package(ulua CONFIG REQUIRED)
target_link_libraries(app PRIVATE ulua::ulua)
```

## Your First Program

```cpp
#include <ulua.hpp>
#include <iostream>
#include <string>

int main() {
    ulua::state s;
    s.open_libraries(ulua::lib::base, ulua::lib::string);

    // Expose a C++ lambda as a Lua global.
    s["greet"] = [](std::string name) {
        return "hello, " + name + "!";
    };

    // Run a Lua script that calls it.
    auto r = s.script(R"(
        return greet("world")
    )");

    if (r.is_error()) {
        std::cerr << "lua error: " << r.error() << '\n';
        return 1;
    }

    std::cout << r.as<std::string>() << '\n'; // hello, world!
    return 0;
}
```

Compile and run:

```sh
cmake -B build && cmake --build build
./build/app
```

## Building from Source

uLua ships a CMake project used primarily for tests and examples:

```sh
git clone https://github.com/can1357/ulua.git
cd ulua
cmake -B build -DULUA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

To enable the LuaJIT acceleration path, build against LuaJIT headers; uLua auto-detects and sets `ULUA_JIT=1` and `ULUA_ACCEL=1`.

## Next Steps

- [State](state.md) — manage the `lua_State*` lifecycle and run scripts.
- [Stack and References](stack-and-references.md) — understand ownership.
- [Userdata](userdata.md) — bind your own C++ types.
- [Cheatsheet](cheatsheet.md) — a condensed reference.
