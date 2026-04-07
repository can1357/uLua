# Cheatsheet

Condensed reference. See the linked docs for detail.

## State

```cpp
#include <ulua.hpp>

ulua::state s;
s.open_libraries(ulua::lib::base, ulua::lib::string, ulua::lib::table);
s.script("print('hi')");
s.script_file("main.lua");
auto chunk = s.load("return 1 + 2");   // compile, call later
```

## Globals

```cpp
s["x"] = 42;
int x = s["x"];

s["cfg"] = s.make_table<ulua::table>();
s["cfg"]["debug"] = true;
```

## Functions

```cpp
ulua::function f = s["my_func"];
auto r = f(1, 2, 3);

if (r.is_error())          std::cerr << r.error();
int  result = r.as<int>();
auto [a, b] = r.as<std::tuple<int, int>>(0);
```

## Tables

```cpp
ulua::table t = s.make_table<ulua::table>();
t["key"] = 99;
t[1]     = "first";

for (auto [k, v] : t) {
    // k, v are ulua::object
}

ulua::freeze_table(t);              // installs __newindex rejector
auto raw = t.at("key", ulua::raw_t{});
```

## C++ Functions in Lua

```cpp
s["add"]       = [](int a, int b) { return a + b; };
s["add_const"] = ulua::constant<&add_function>();
s["method"]    = ulua::constant<&MyClass::method>();

s["print2"] = ulua::overload<
    [](int n)         { /* int form */ },
    [](std::string s) { /* string form */ }
>{};
```

## Userdata

```cpp
struct Vec2 { double x, y; double length() const; };

namespace ulua {
template<> struct user_traits<Vec2> {
    static constexpr auto name = "Vec2";
    static constexpr auto fields = std::tuple{
        member<&Vec2::x>("x"),
        member<&Vec2::y>("y"),
        member<&Vec2::length>("length"),
    };
};
}

s["Vec2"] = [](double x, double y) { return Vec2{x, y}; };
```

## Named Arguments

```cpp
using namespace ulua;
void spawn(named<int, "count"_n> n,
           named_opt<std::string, "name"_n> name);
```

## Stack and References

```cpp
ulua::stack::push<int>(L, 42);
int n = ulua::stack::pop<int>(L);

ulua::stack_reference    sr{L, ulua::stack::top_t{}};  // transient
ulua::registry_reference rr{L, ulua::stack::top_t{}};  // long-lived
rr.push();
```

## Coroutines

```cpp
auto co = ulua::coroutine::create(s["worker"]);
int status = co.resume(1, 2);   // 0 / LUA_YIELD / error code
bool inside = ulua::coroutine::running(L);
```

## Environments

```cpp
ulua::environment env{L, ulua::create{}, s.globals()};
env["local_thing"] = 5;
env.set_on(s["sandboxed_fn"]);
```

## FFI (LuaJIT)

```cpp
#if ULUA_JIT
ulua::ffi::cdef(s, "typedef struct { double x, y; } vec2_t;");
ulua::ffi::set_metatable(s, "vec2_t", vec2_mt);
#endif
```

## Errors

```cpp
auto r = s.script("bad code");
if (!r.is_success()) std::cerr << r.error();

// Inside a C callback:
ulua::error(L, "bad value %d", n);
ulua::arg_error(L, 1, "expected positive");
ulua::type_error(L, 2, "expected Vec2");
```

## Garbage Collection / Panic

```cpp
s.collect_garbage();
s.set_panic([](lua_State* L) -> int { std::abort(); });
```
