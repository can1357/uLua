# Values and Conversions

uLua bridges C++ and Lua values through a single extension point: the `type_traits<T>` template. Every push, get, and type-check goes through it, including the containers and tuples shipped with the library.

## The `type_traits<T>` protocol

```cpp
namespace ulua {
    template<typename T>
    struct type_traits {
        // Push a value of type T onto L's stack. Must push exactly one slot
        // unless the trait explicitly documents multi-slot behavior (tuples).
        static void push(lua_State* L, const T& value);

        // Non-destructive type test. idx is mutated to point past the
        // consumed slot(s); returning false leaves the stack untouched.
        static bool check(lua_State* L, int& idx);

        // Read and advance idx. Called after check() or when the caller
        // is willing to raise on mismatch.
        static T get(lua_State* L, int& idx);
    };
}
```

`check` and `get` advance `idx` so composite traits (tuple, pair, variant) can chain through several stack positions in one call.

## Adding a custom conversion

Specialize `type_traits` in namespace `ulua`. This example marshals a `Color` as a 3-field Lua table:

```cpp
struct Color { float r, g, b; };

namespace ulua {
    template<>
    struct type_traits<Color> {
        static void push(lua_State* L, const Color& c) {
            stack::create_table(L, reserve_table{0, 3});
            stack::set_field(L, -1, "r", c.r);
            stack::set_field(L, -1, "g", c.g);
            stack::set_field(L, -1, "b", c.b);
        }
        static bool check(lua_State* L, int& idx) {
            bool ok = lua_istable(L, idx);
            ++idx;
            return ok;
        }
        static Color get(lua_State* L, int& idx) {
            int i = idx++;
            return {
                stack::get_field<float>(L, i, "r"),
                stack::get_field<float>(L, i, "g"),
                stack::get_field<float>(L, i, "b"),
            };
        }
    };
}
```

For user types managed as userdata, prefer `user_traits<T>` — see [userdata.md](userdata.md).

## Built-in conversions

| C++ type | Lua representation | Notes |
|---|---|---|
| `int`, `long`, `uint64_t`, … | `number` | any signed/unsigned integer width |
| `enum`/`enum class` | `number` | forwarded to the underlying integer trait |
| `float`, `double`, `long double` | `number` | |
| `bool` | `boolean` | |
| `const char*`, `std::string_view`, `std::string` | `string` | |
| `nil_t` | `nil` | tag type, `ulua::nil` is the instance |
| `cfunction_t` (alias of `lua_CFunction`) | `function` | raw C closure |
| `light_userdata` | `lightuserdata` | wraps a `void*` |
| `userdata_value<T>` | `userdata` | explicit userdata emplace |
| `std::optional<T>` | `nil` or `T` | nullable arguments and returns |
| `std::tuple<T...>` | N stack slots | multi-return and multi-arg packing |
| `std::pair<A, B>` | 2 stack slots | shorthand for a 2-tuple |
| `std::variant<T...>` | one of `T...` | type-test dispatch in order |
| `std::nullopt_t`, `std::nullptr_t` | `nil` | push-only convenience |

## `value_type` and diagnostics

```cpp
enum class value_type {
    nil, boolean, light_userdata, number, string,
    table, function, userdata, thread,
    cdata,   // LuaJIT only
};

std::string_view name = ulua::type_name(ulua::value_type::table); // "table"
```

Use it when building error messages or branching on the runtime type of a `ulua::object`.

## Usage patterns

Push a primitive through a state proxy:

```cpp
ulua::state s;
s["answer"]  = 42;
s["pi"]      = 3.14159;
s["greet"]   = std::string_view{"hello"};
s["missing"] = ulua::nil;
```

Multi-return collected into a tuple:

```cpp
ulua::function divmod = s["divmod"];           // function divmod(a,b) return a//b, a%b end
auto [q, r] = divmod(17, 5).as<std::tuple<int, int>>();
```

Optional arguments — distinguish "absent" from "explicit nil or zero":

```cpp
s["lookup"] = [](std::string key, std::optional<std::string> fallback) {
    auto hit = cache_find(key);
    if (hit) return *hit;
    return fallback.value_or("<missing>");
};
```

Polymorphic input via `std::variant`:

```cpp
s["stringify"] = [](std::variant<int, double, std::string> v) {
    return std::visit([](auto&& x) { return fmt::format("{}", x); }, v);
};
```

`check` is order-sensitive for `variant`: the first alternative that accepts the slot wins. List more specific types first.

See also: [tables.md](tables.md), [functions-and-closures.md](functions-and-closures.md), [userdata.md](userdata.md).
