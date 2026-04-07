# Stack and References

Lua's C API is built around a per-state value stack. Every value passed between C++ and Lua lives, at some point, in a numbered slot on that stack. uLua exposes that model directly through the `stack::` namespace and adds two RAII reference types that keep values alive safely.

## The Stack Model

A `lua_State` owns a stack of Lua values. Slots are 1-indexed from the bottom and also addressable from the top with negative indices: `-1` is the topmost value, `-2` the next, and so on. `lua_gettop(L)` returns the current depth.

uLua distinguishes three index kinds:

- **Absolute**: a positive index fixed relative to the stack base.
- **Relative**: a negative index counted from the top, sensitive to later pushes/pops.
- **Special**: pseudo-indices like `LUA_REGISTRYINDEX`.

Convert between them:

```cpp
int abs_i = ulua::stack::abs(L, -1);      // resolve top to absolute
int rel_i = ulua::stack::rel(L, 3);       // negative form
bool r = ulua::stack::is_relative(i);
bool a = ulua::stack::is_absolute(i);
bool s = ulua::stack::is_special(i);
int top = ulua::stack::top(L);
```

## Push, Get, Pop, Check

```cpp
ulua::stack::push<int>(L, 42);
int x = ulua::stack::get<int>(L, -1);     // non-consuming read
int y = ulua::stack::check<int>(L, 1);    // raises type_error if wrong
int z = ulua::stack::pop<int>(L);         // read top and drop it
```

`check<T>` is the variant you want inside C functions called from Lua: it emits a proper `argument #N has wrong type` error on mismatch.

## Table Fields

```cpp
ulua::stack::create_table(L, ulua::reserve_table{0, 4});
ulua::stack::set_field(L, -1, "name", std::string{"ulua"});
auto name = ulua::stack::get_field<std::string>(L, -1, "name");
ulua::stack::set_field(L, -1, "raw", 1, ulua::raw_t{}); // bypass metamethods
```

`reserve_table{arr, rec}`, `reserve_array{n}`, and `reserve_records{n}` are allocation hints that the underlying `lua_createtable` consumes. Supplying accurate values avoids rehashes on growing tables.

## References

A `reference_base` subclass keeps a Lua value alive without forcing you to track stack depth by hand. The `Reference` concept matches any such type:

```cpp
template <typename T>
concept Reference = std::is_base_of_v<ulua::reference_base, T>;
```

### `stack_reference`

Binds to a slot on the stack. On destruction, if it owns the slot, it pops it. Use for short-lived values that never outlive the current C function frame.

```cpp
{
    ulua::stack::push<int>(L, 7);
    ulua::stack_reference r{L, ulua::stack::top_t{}}; // claim top
    // use r...
}   // slot popped here
```

Alternate constructors:

- `stack_reference(L, i)` — owning, pops slot `i` on destroy.
- `stack_reference(L, i, weak_t{})` — non-owning view; never pops.
- `stack_reference(L, stack::top_t{})` — claim current top.
- `stack_reference(nullref)` — empty.

### `registry_reference`

Stores the value in `LUA_REGISTRYINDEX` via `luaL_ref` and releases it via `luaL_unref`. Use for long-lived handles that cross function boundaries: cached functions, module tables, callback closures.

```cpp
ulua::stack::push(L, some_fn);
ulua::registry_reference handle{L, ulua::stack::top_t{}};
// handle survives until destroyed/reset
handle.push();          // re-push onto the stack
```

### Common interface

```cpp
r.state();              // lua_State*
r.push();               // push value onto stack
r.valid();              // has a live target
r.release();            // relinquish ownership, return raw slot/key
r.reset();              // drop reference now
r.slot();               // stack_reference only
r.registry_key();       // registry_reference only
```

Both are move-only; copying would double-unref or double-pop.

## Registry Primitives

If you need the raw registry dance without the wrapper:

```cpp
ulua::stack::push(L, value);
ulua::reg_key k = ulua::stack::pop_reg(L);
ulua::stack::push_reg(L, k);
ulua::stack::unref(L, k);
```

## Comparing References

`equals(a, b)` invokes Lua's raw-equality semantics and works for any combination of reference types:

```cpp
if (ulua::equals(cached_fn, current_fn)) { /* ... */ }
std::size_t n = ulua::length(some_table);
```

## Debugging the Stack

```cpp
std::string dump = ulua::stack::dump_stack(L);
std::string one  = ulua::stack::to_string(L, -1);
```

Use these inside breakpoints or log lines while developing bindings.
