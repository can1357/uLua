# FFI Integration

uLua can push C++ types as LuaJIT `cdata` instead of standard userdata.
This eliminates the `userdata_wrapper<T>` tag overhead, produces objects
the JIT compiler can trace through, and lets Lua code interact with the
type via the LuaJIT FFI rather than __index dispatch.

**LuaJIT only.** Gated by the `ULUA_JIT` macro.

See also: [userdata.md](userdata.md), [metatables.md](metatables.md).

## Prerequisites

The FFI path requires building against the LuaJIT **source tree**, not a
standard install. uLua needs the internal headers (`lj_obj.h`, `lj_ctype.h`,
etc.) that are not shipped in a typical `luajit-dev` package.

| Macro | Meaning |
|---|---|
| `ULUA_JIT` | Set to `1` when LuaJIT internal headers are detected. |
| `ULUA_ACCEL` | Set to `1` when the accelerated TValue path is enabled. |
| `ULUA_NO_ACCEL` | Define this to disable the accel path even when headers exist. |

If `ULUA_JIT` is 0, every FFI call is a compile-time no-op; the userdata
fallback is used transparently.

## Core API

### `ffi::cdef`

```cpp
ulua::state s;
ulua::ffi::cdef(s, R"(
    typedef struct { double x, y; } vec2_t;
)");
```

Parses C declarations and registers them with the LuaJIT FFI subsystem.
Equivalent to calling `ffi.cdef` from Lua.

### `ffi::set_metatable`

```cpp
ulua::table mt = s.make_table<ulua::table>();
mt["__add"] = [](const Vec2& a, const Vec2& b) {
    return Vec2{a.x + b.x, a.y + b.y};
};
ulua::ffi::set_metatable(s, "vec2_t", mt);
```

Attaches a metatable to a ctype via `ffi.metatype`. Once attached the
metatable is permanent — LuaJIT does not allow replacing it.

## Opting a C++ type into FFI storage

Inherit `user_traits<T>` from `ulua::ctype_t`:

```cpp
template<> struct ulua::user_traits<Vec2> : ulua::ctype_t {
    static constexpr const char* name = "vec2_t";
    static constexpr auto cdef = R"(
        typedef struct { double x, y; } vec2_t;
    )";
    static constexpr auto fields = std::tuple{
        ulua::member<&Vec2::x>("x"),
        ulua::member<&Vec2::y>("y"),
    };
};
```

When `user_traits<T>` satisfies the `UserCType` concept (inherits
`ctype_t`), uLua stores the object inside a `cdata` allocation
(`cdataptr`) instead of a userdata block. The `userdata_wrapper` tag is
not present — type identity is tracked via the LuaJIT type id.

### Inline cdef

If `user_traits<T>::cdef` is defined (detected by the `CTypeHasInlineCDef`
concept), uLua calls `ffi::cdef` automatically the first time the type is
pushed. No manual `cdef` call is needed.

### `ctype_id_cache<T>`

Every FFI type has a numeric type id. Looking it up by name
(`lj_ctype_getname`) is not free. `ctype_id_cache<T>` is a per-state cache
that resolves the id once and reuses it on subsequent push/get operations.

## Performance characteristics

| Aspect | userdata | cdata (FFI) |
|---|---|---|
| Allocation | userdata block + wrapper header | cdata allocation, no header |
| JIT trace | opaque; calls exit trace | transparent to JIT |
| GC | full `__gc` metamethod call | cdata finalizer (faster) |
| Metatable | per-type, auto-generated | permanent `ffi.metatype` |

The FFI path is strictly faster when the LuaJIT JIT compiler is active.
Under the interpreter the difference is smaller. For types that are
allocated and discarded at high frequency (vectors, colors, small structs),
the reduced allocation overhead and GC pressure make a measurable
difference.

## Disabling the accel path

If you want LuaJIT FFI support but not the TValue-level acceleration:

```cpp
#define ULUA_NO_ACCEL
#include <ulua.hpp>
```

This sets `ULUA_ACCEL=0` while keeping `ULUA_JIT=1`, so `ffi::cdef` and
`ffi::set_metatable` remain available but stack operations use the standard
Lua C API.
