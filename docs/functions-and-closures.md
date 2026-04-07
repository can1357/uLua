# Functions and Closures

uLua bridges callables in both directions: calling Lua functions from C++, and exposing C++ callables to Lua as first-class functions.

| Type | Backed by | Use when |
|---|---|---|
| `ulua::function` | `registry_reference` | storing Lua functions long-term |
| `ulua::stack_function` | `stack_reference` | transient calls inside a C entry point |

Both alias `basic_function<Ref>` and share the same call operator.

## Calling Lua from C++

Grab a function out of the state or any table:

```cpp
ulua::state s;
s.open_libraries(ulua::lib::base, ulua::lib::string);
s.script("function greet(who) return 'hello, ' .. who end");

ulua::function greet = s["greet"];
// or: auto greet = s["greet"].as<ulua::function>();
```

Invoke it. The call operator returns a `function_result`, an RAII wrapper over the pcall stack slice:

```cpp
ulua::function_result r = greet("world");
if (!r) {
    std::cerr << "lua error: " << r.error() << '\n';
    return;
}
std::string msg = r;           // implicit decay, asserts on error
```

### Extracting values

`function_result` offers several extraction paths:

```cpp
auto r = s["math_ops"](3, 4);

int first  = r;                                  // implicit, asserts on error
int second = r.as<int>(1);                       // explicit, indexed
double one = r.as<double>();                     // index 0 by default

auto [sum, prod] = r.as<std::tuple<int, int>>(); // multi-return as tuple
```

Implicit conversion to `std::tuple<...>` is also supported, which is convenient with structured bindings:

```cpp
auto [x, y, z] = std::tuple<int, int, int>(s["triple"](1));
```

### Error handling

```cpp
auto r = s["maybe_fail"]();
if (r.is_error()) {
    log("lua: %s", r.error().c_str());
} else {
    process(r.as<int>());
}
```

`function_result` is move-only and pops its slice on destruction. Do not store it past the next call on the same state; extract the values you need into owned storage first.

## Calling C++ from Lua

uLua accepts several kinds of callables on the assignment side:

### Stateless lambda

The zero-overhead path — pushed as a `lua_CFunction` with no upvalues:

```cpp
s["add"] = [](int a, int b) { return a + b; };
```

### Stateful lambda

Captures are boxed as userdata in the closure's upvalue and released by a generated `__gc`:

```cpp
s["counter"] = [n = 0]() mutable { return ++n; };
```

### Free function pointer

`constant<&fn>()` produces a compile-time constant closure — the function address is baked in, so no upvalues are needed:

```cpp
int my_fn(int a, int b) { return a * b; }
s["mul"] = ulua::constant<&my_fn>();
```

### Member function pointer

The first Lua argument is taken as `self`, which must convert to the owning userdata type:

```cpp
struct Counter { int n = 0; int inc(int by) { return n += by; } };
s["Counter.inc"] = ulua::constant<&Counter::inc>();
```

### Raw `lua_CFunction`

For full manual control you can hand over a C function directly:

```cpp
s["raw"] = (lua_CFunction)&my_c_api;
```

## Returning multiple values

Return a tuple — each element is pushed as a separate result:

```cpp
s["divmod"] = [](int a, int b) {
    return std::tuple{a / b, a % b};
};
```

For hand-managed return counts, push into the stack directly and return a `push_count`:

```cpp
s["variadic"] = [](ulua::state_view L) -> ulua::push_count {
    ulua::stack::push(L, 1);
    ulua::stack::push(L, 2);
    ulua::stack::push(L, 3);
    return ulua::push_count{3};
};
```

## Overload dispatch

`ulua::overload` composes several callables and dispatches based on argument types at call time:

```cpp
s["stringify"] = ulua::overload{
    [](int i)              { return fmt::format("int {}", i); },
    [](double d)           { return fmt::format("num {}", d); },
    [](std::string_view s) { return fmt::format("str {}", s); },
};
```

The first candidate whose `type_traits::check` succeeds on every argument wins; if none match, a Lua type error is raised. Order specific-to-general.

## `caller_reference`

A pseudo-argument that captures the function currently executing — useful for introspection, error reporting, or building `debug.getinfo`-like helpers:

```cpp
s["whoami"] = [](ulua::caller_reference self) {
    return ulua::to_string(self);
};
```

The caller reference is filled in by uLua's invocation shim; Lua code does not pass anything for it.

See also: [values-and-conversions.md](values-and-conversions.md), [tables.md](tables.md), [userdata.md](userdata.md).
