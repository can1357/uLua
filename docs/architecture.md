# Architecture

ulua stays close to Lua's execution model. The stack is still the source of truth. The library mostly exists to make stack operations typed, explicit, and less error-prone.

## Design center

The core design choice is visible almost everywhere: ulua wraps Lua values as references and proxies instead of copying them into a separate object model.

That has a few consequences.

- Most abstractions are thin and cheap.
- Lifetime still matters because many wrappers refer back to a live `lua_State`.
- The public surface feels closer to the Lua C API than to a scripting engine with its own runtime.

## Main layers

### `state` and `state_view`

`state_view` is the non-owning façade over `lua_State*`. It owns most of the high-level helpers: opening libraries, creating tables and metatables, loading scripts, running scripts, and accessing globals.

`state` adds ownership. It creates and closes the Lua state and exposes reset helpers.

### `stack`

`stack.hpp` is the low-level substrate. It handles push, pop, typed access, table field access, stack slot conversion, raw registry access, and assorted Lua utility calls.

Most other modules eventually route through this layer.

### references and lazy wrappers

`stack_reference` and `registry_reference` represent Lua values that already exist. `basic_object`, `basic_table`, `basic_function`, and a few other wrappers build on top of those references.

The `lazy_*` helpers add three behaviors without creating a second representation:

- cast a referenced Lua value to a C++ type
- index into tables
- invoke callable Lua values

That is why so many types in ulua are small wrapper templates over a reference base.

### functions and closures

`function.hpp` owns two related pieces:

- `function_result`, which models the result slice returned by a protected call
- `detail::pcall`, which performs the protected call and constructs that result object

`closure.hpp` is the other half. It turns C++ callables into Lua-callable closures and handles the stack marshaling needed on both sides.

### tables and environments

`table.hpp` adds typed table wrappers, proxy-based indexing, iteration, and a helper for freezing tables.

`environment.hpp` builds on that. It creates environment tables and can attach them to loaded chunks.

### userdata and metatables

`userdata.hpp` defines the storage and trait machinery for exposing user types. `userdata_metatable.hpp` is the higher-level metatable generator that uses `user_traits<T>` to expose fields, methods, metamethods, and a few convenience patterns.

This is the part of the library with the most policy baked in. It is also the part that gives ulua a lot of its value.

### optional LuaJIT extensions

`ffi.hpp` and parts of `lua_types.hpp` expose LuaJIT-specific behavior. These paths depend on LuaJIT private headers, not just the public API headers. When those private headers are missing, ulua now compiles in a portable mode and leaves the LuaJIT-only extensions out.

That tradeoff is intentional. A reduced feature set is better than a broken include path.

## Public entry point

`include/ulua.hpp` is the umbrella header. It re-exports the public modules in one place, so consumers can choose between a single include and more selective includes.
