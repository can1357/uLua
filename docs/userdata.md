# Userdata

uLua exposes C++ types to Lua via userdata. The binding is opt-in: a type
becomes visible to Lua only after you specialize `ulua::user_traits<T>` or
define a `lua_traits` alias inside the type itself.

See also: [metatables.md](metatables.md), [architecture.md](architecture.md).

## Opting in

Specialize `ulua::user_traits<T>`:

```cpp
#include <ulua.hpp>

struct Vec2 {
    double x = 0, y = 0;
    double length() const { return std::sqrt(x*x + y*y); }
};

template<> struct ulua::user_traits<Vec2> {
    static constexpr const char* name = "Vec2";
    static constexpr auto fields = std::tuple{
        ulua::member<&Vec2::x>("x"),
        ulua::member<&Vec2::y>("y"),
        ulua::member<&Vec2::length>("length"),
    };
};
```

Alternatively, embed the traits inside the type:

```cpp
struct Vec2 {
    double x, y;
    using lua_traits = struct {
        static constexpr const char* name = "Vec2";
        static constexpr auto fields = std::tuple{
            ulua::member<&Vec2::x>("x"),
            ulua::member<&Vec2::y>("y"),
        };
    };
};
```

## Pushing and retrieving

```cpp
ulua::state s;
s.open_libraries(ulua::lib::base);

// By value: uLua stores the object inline inside the userdata block.
// __gc runs the destructor.
s["v"] = Vec2{1.0, 2.0};

// By pointer: uLua stores a raw pointer. The caller owns the object,
// no destructor is invoked.
Vec2 stable{3.0, 4.0};
s["p"] = &stable;

Vec2&       ref  = s["v"];  // reference into the userdata block
const Vec2& cref = s["v"];
Vec2*       ptr  = s["p"];  // the stored pointer
```

Storage is tracked by `ulua::userdata_storage::value` vs `::pointer`. The
`userdata_wrapper<T>` header placed at the front of every userdata block
carries a type tag used for a type-safe downcast on every access — a
mismatch raises a Lua error rather than reinterpret-casting.

Define `ULUA_CONST_CORRECT=1` before including `<ulua.hpp>` to also track
const-ness in the wrapper; getting a `T&` from a `const`-pushed value will
then raise instead of silently stripping const.

## Field descriptors

`ulua::member<Ptr>(name)` binds a pointer-to-member. It dispatches on the
pointer type:

```cpp
template<> struct ulua::user_traits<Widget> {
    static constexpr const char* name = "Widget";
    static constexpr auto fields = std::tuple{
        ulua::member<&Widget::id>("id", ulua::readonly_t{}),  // read-only field
        ulua::member<&Widget::label>("label"),                // r/w field
        ulua::member<&Widget::redraw>("redraw"),              // method
        ulua::property("area",                                // computed
            [](const Widget& w) { return w.w * w.h; }),
        ulua::property("scale",                               // computed r/w
            [](const Widget& w) { return w.scale; },
            [](Widget& w, double s) { w.scale = s; }),
        ulua::static_member("VERSION", 3),                    // type-level const
    };
};
```

- `member<&T::field>` — data member, read/write by default, `readonly_t{}` locks it.
- `member<&T::method>` — member function, bound as a Lua method (self as arg 1).
- `property(name, get[, set])` — computed accessor, takes any callable.
- `static_member(name, v)` — attached to the type's `__index`, no instance needed.
- `constant_getter<V>(name)` — compile-time constant, no storage.
- `bytecode_property(name, blob)` — precompiled Lua accessor.

## Constructors

uLua does not auto-generate constructors. Expose a factory:

```cpp
s["Vec2"] = [](double x, double y) { return Vec2{x, y}; };
// Lua: local v = Vec2(1, 2); print(v.length())
```

Use `ulua::overload<F1, F2, ...>` to dispatch on argument types when you
need multiple constructors.
