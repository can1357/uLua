# Tables

uLua exposes Lua tables through two parallel types that differ only in how they anchor the underlying `table` value on the Lua stack.

| Type | Backed by | Lifetime | Use when |
|---|---|---|---|
| `ulua::table` | `registry_reference` | persistent, survives any stack churn | storing tables in C++ fields, passing across frames |
| `ulua::stack_table` | `stack_reference` | tied to a fixed slot on the current stack | short-lived locals inside a C function |

Both are aliases for `basic_table<Ref>` and share the same API.

## Creating tables

Through the owning state:

```cpp
ulua::state s;
auto cfg = s.make_table<ulua::table>(ulua::reserve_table{/*array*/ 0, /*records*/ 4});
cfg["host"] = "localhost";
cfg["port"] = 5432;
```

Directly from a `lua_State*`:

```cpp
ulua::table t{L, ulua::create{ulua::reserve_table{8, 0}}};
```

Reservation hints pre-size the underlying table to avoid rehashes:

```cpp
ulua::reserve_table{arr_slots, rec_slots};   // both parts
ulua::reserve_array{n};                       // array-only, arr=n, rec=0
ulua::reserve_records{n};                     // record-only, arr=0, rec=n
```

They are advisory — passing zeros is always valid.

## Indexing with `table_proxy`

`t["key"]` does not touch the stack immediately. It returns a `table_proxy<Key, Raw=false>` that defers the actual `lua_gettable`/`lua_settable` until it is used as an rvalue or assigned to.

Read by assigning to a typed variable:

```cpp
int        port = cfg["port"];
std::string host = cfg["host"];
auto       sub  = cfg["sub"].get<ulua::table>();   // explicit form
```

Write by assigning through the proxy:

```cpp
cfg["timeout"]  = 30;
cfg["features"] = s.make_table<ulua::table>();
```

### Raw access

By default indexing goes through `__index`/`__newindex` metamethods. Use `at(key, raw_t{})` to bypass them — equivalent to `rawget`/`rawset`:

```cpp
int v = cfg.at("port", ulua::raw_t{});
cfg.at("port", ulua::raw_t{}) = 6432;
```

### Nested access

Chained proxies walk through subtables without building intermediate `ulua::table` handles:

```cpp
cfg["db"]["pool"]["max"] = 16;
int mx = cfg["db"]["pool"]["max"];
```

Each step materializes one stack slot and releases it after the final read or write.

## Iteration

`basic_table` exposes a forward iterator pair that wraps `lua_next`. Keys and values are returned as `ulua::object` — a generic typed reference — so you can re-dispatch on their runtime `value_type`:

```cpp
for (auto&& [key, value] : cfg) {
    if (key.type() == ulua::value_type::string &&
        value.type() == ulua::value_type::number) {
        std::cout << key.as<std::string>() << " = "
                  << value.as<double>() << '\n';
    }
}
```

Iterator semantics:

- Forward-only; `operator++` calls `lua_next` against the table slot.
- `end()` is a sentinel; the comparison checks whether the last `lua_next` returned zero.
- Do not mutate the table's key set mid-iteration — same restriction as plain `pairs`.

## Freezing

`freeze_table(t)` installs a metatable with a rejecting `__newindex`:

```cpp
ulua::freeze_table(cfg);
cfg["new_key"] = 1;   // raises "attempt to modify a frozen table"
```

Lua 5.1 caveat: `__newindex` only fires for keys that are **not already present** in the raw table. Existing keys can still be reassigned after freezing. If you need full immutability, clear the table into a fresh empty one and proxy reads through `__index`, or wrap writes in raw tests yourself.

## Length

`length(ref)` forwards to `lua_objlen` (Lua 5.1 `#`):

```cpp
ulua::table arr = s.make_table<ulua::table>(ulua::reserve_array{3});
arr[1] = "a"; arr[2] = "b"; arr[3] = "c";
std::size_t n = ulua::length(arr);   // 3
```

For tables with holes the result is any border, as in plain Lua.

See also: [values-and-conversions.md](values-and-conversions.md), [functions-and-closures.md](functions-and-closures.md).
