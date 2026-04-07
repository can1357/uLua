# Auto-generated Metatables

When a C++ type with `ulua::user_traits<T>` is first pushed to a Lua state,
uLua generates a metatable for it and caches it under `T::name` in the
registry. Subsequent pushes reuse the cached metatable. Generation is driven
by C++20 concepts that introspect the type at compile time.

See also: [userdata.md](userdata.md), [architecture.md](architecture.md).

## What is generated

| Metamethod | Condition |
|---|---|
| `__index` | Always. Dispatches through `__indexref` if `fields` is defined, then falls back to the metatable itself. |
| `__newindex` | Always. Dispatches through `__newindexref` if writable fields exist. |
| `__gc` | When `T` is not trivially destructible. Calls the destructor. |
| `__tostring` | When `T` defines `to_string()`. Otherwise prints `<TypeName: 0xADDR>`. |
| `__name` | Always set to `user_traits<T>::name`. |
| `__eq` | When `operator==` exists for `T`. |
| `__lt` | When `operator<` exists for `T`. |
| `__le` | When `operator<=` exists for `T`. |
| `__len` | When `T` has a `length()` method, `std::size(T)`, or satisfies iterable. |
| `__pairs` | When `T` is key/value iterable (has `begin()`/`end()` returning `pair`-like). |
| `__ipairs` | When `T` is index iterable (contiguous or integer-keyed). |
| `__unm` | When unary `operator-` exists. |
| `__concat` | When `operator+` exists for string-like types, or explicit opt-in. |
| `__add` | When `operator+` exists. |
| `__sub` | When `operator-` (binary) exists. |
| `__mul` | When `operator*` exists. |
| `__div` | When `operator/` exists. |
| `__idiv` | When integer division is defined. |
| `__mod` | When `operator%` exists. |
| `__pow` | When a power operation is defined. |
| `__metatable` | Set to `false` to prevent Lua-side tampering via `getmetatable()`. |

Detection uses C++20 concepts defined in `detail::` — for example
`detail::HasEquality<T>`, `detail::HasLength<T>`, `detail::Iterable<T>`.
Only operations that are valid at compile time produce entries; there is no
runtime dispatch overhead for unused metamethods.

## Overriding individual metamethods

Override any auto-generated entry via `user_traits<T>::metatable`:

```cpp
template<> struct ulua::user_traits<Vec2> {
    static constexpr const char* name = "Vec2";
    static constexpr auto fields = std::tuple{
        ulua::member<&Vec2::x>("x"),
        ulua::member<&Vec2::y>("y"),
    };
    static constexpr auto metatable = std::tuple{
        ulua::property(ulua::meta::add, [](const Vec2& a, const Vec2& b) {
            return Vec2{a.x + b.x, a.y + b.y};
        }),
        ulua::property(ulua::meta::tostring, [](const Vec2& v) {
            return std::format("({}, {})", v.x, v.y);
        }),
    };
};
```

The `metatable` tuple is iterated at registration time. Entries keyed by a
`meta` enumerator replace the auto-generated metamethod; other entries are
ignored for metatable purposes and treated as extra fields.

## Field dispatch internals

When `fields` is defined, the metatable contains two hidden tables:

- `__indexref` — maps field name strings to getter closures.
- `__newindexref` — maps field name strings to setter closures.

The generated `__index` handler first looks up the key in `__indexref`. On
hit it calls the getter closure. On miss it falls through to the metatable
itself. This makes field access O(1) — a single table lookup per access —
while still allowing method resolution through the normal `__index` chain.

`__newindex` works identically through `__newindexref`.

## The `meta` enum

```cpp
namespace ulua {
    enum class meta {
        index, newindex, gc, tostring, name,
        eq, lt, le, len, pairs, ipairs,
        unm, concat, add, sub, mul, div, idiv, mod, pow,
        metatable,
    };
}
```

`ulua::metafield_name(meta::add)` returns `"__add"`. This is useful for
runtime introspection or building custom metatables by hand via the stack
API. See [stack](stack.md) for `stack::create_metatable` and related calls.
