# State Management

Every Lua program runs inside a `lua_State`. uLua wraps that handle in two classes: `ulua::state` (owning) and `ulua::state_view` (non-owning).

## `state` vs `state_view`

`ulua::state` constructs a fresh `lua_State` in its constructor and calls `lua_close` on destruction. Use it at the top of `main()` or as a member of an application-owning object.

`ulua::state_view` wraps an existing `lua_State*` without taking ownership. Use it inside C functions pushed into Lua, or when embedding uLua alongside another binding layer that already owns the state.

```cpp
ulua::state s;                          // owns a new lua_State
ulua::state_view v{existing_L};         // non-owning view
```

`state` inherits from `state_view`, so every API below works on both unless noted.

## Library Loading

Open standard libraries individually or in a single variadic call:

```cpp
s.open_libraries(
    ulua::lib::base,
    ulua::lib::string,
    ulua::lib::table,
    ulua::lib::math
);
```

Available modules: `base`, `package`, `string`, `table`, `math`, `io`, `os`, `debug`, `bit`. LuaJIT builds also expose `lib::ffi` and `lib::jit`.

## Running Scripts

```cpp
auto r = s.script("return 1 + 2");      // load + execute
int n = r;                              // implicit: throws via ulua::error on failure

s.script_file("config.lua");            // load + execute a file

auto loaded = s.load("return 1 + 2");   // load only; returns a callable load_result
auto result = loaded();                 // execute later
```

`script()` and `script_file()` return a `function_result`. `load()` and `load_file()` return a `load_result` holding a compiled chunk you can invoke multiple times. See [Error Handling](error-handling.md) for failure checking.

## Globals

`state::operator[]` returns a `table_proxy` onto `_G`:

```cpp
s["x"] = 42;
int x = s["x"];

s["config"] = s.make_table<ulua::table>();
s["config"]["debug"] = true;
```

`s.globals()` returns the globals table explicitly when you need a first-class reference.

## Creating Tables

```cpp
ulua::table t = s.make_table<ulua::table>();
t["name"] = "ulua";
t[1]      = 10;
```

Pass a `reserve_table{arr, rec}` hint if you know the final array/record counts; uLua forwards it to `lua_createtable`.

## Metatables

Register a named metatable once, reuse it by name:

```cpp
auto mt = s.make_metatable("Vec2");
mt["__tostring"] = [](ulua::stack_function /* self */) {
    return std::string{"Vec2"};
};

auto mt2 = s.get_metatable("Vec2");     // retrieves the same table
```

For userdata types declared via `user_traits<T>`, uLua builds and caches the metatable automatically on first use. See [Userdata](userdata.md).

## Garbage Collection

```cpp
s.collect_garbage();                    // full cycle, equivalent to collectgarbage("collect")
```

Call between heavy allocations or in idle frames.

## Panic Handler

Lua panics on unprotected errors. Install a handler to log or abort cleanly:

```cpp
s.set_panic([](lua_State* L) -> int {
    std::fprintf(stderr, "lua panic: %s\n", lua_tostring(L, -1));
    std::abort();
});
```

In normal use, scripts run under pcall via `script()`/`load()` and errors surface through `function_result`, so the panic path is only hit for genuine API misuse.
