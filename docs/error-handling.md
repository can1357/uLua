# Error Handling

Lua errors are not C++ exceptions. When Lua code fails, the error propagates through `longjmp` (or C++ exceptions, depending on how Lua was built) to the nearest `lua_pcall`. uLua hides that machinery behind result types and noreturn helpers, but you should understand the model before catching misuse.

## The Lua Error Model

- Unprotected errors unwind through the runtime and hit the panic handler if nothing catches them.
- Protected calls (`lua_pcall`, and everything uLua's `script`/`load`/`function_result` does) capture the error value and leave it on the stack.
- An error value is typically a string, but may be any Lua value.

## Raising Errors from C++

Inside a C function bound to Lua — a lambda, a `constant<&fn>()`, or a method of a userdata — use uLua's noreturn helpers. They wrap `luaL_error` and never return:

```cpp
[[noreturn]] void my_fn(ulua::state_view s, int index) {
    if (index < 0)
        ulua::error(s.L(), "negative index %d not allowed", index);
}
```

Three variants exist:

```cpp
ulua::error(L, "format %s", msg);                 // generic error
ulua::arg_error(L, 1, "expected positive");       // argument #N has wrong value
ulua::type_error(L, 2, "expected %s", "Vec2");    // argument #N type mismatch
```

All three are marked `[[noreturn]]`. The compiler treats subsequent code as unreachable, so you do not need a trailing `return`.

## `function_result`

`script()`, `script_file()`, and any call on a `ulua::function` return a `function_result`. It owns the stack region where the return values live.

```cpp
auto r = s.script("return 1, 2, 3");

if (r.is_error()) {
    std::cerr << "script failed: " << r.error() << '\n';
    return;
}

std::size_t n = r.size();          // 3
int a = r.as<int>(0);
int b = r.as<int>(1);
bool is_num = r.is<int>(2);

std::string dbg = r.to_string();   // debug rendering of all returns
```

### Implicit conversion

Converting a `function_result` to a value type calls `assert()` internally, which raises a `ulua::error` if the result is an error. This makes single-value extraction concise:

```cpp
int n = s.script("return 2 + 2"); // throws via ulua::error on failure
```

Use `is_success()` / `is_error()` explicitly when you want to handle the failure path locally.

### Caveat: `<cassert>` macro collision

`function_result::assert()` is a member function. If a translation unit includes `<cassert>` (or any header that does), the `assert` identifier becomes a preprocessor macro and will mangle the member call site. Workarounds, in order of preference:

```cpp
if (r.is_error()) { /* handle */ }   // don't call assert() at all

r.template assert();                 // qualify to defeat macro expansion

#undef assert                        // last resort, TU-local
r.assert();
```

Prefer the first form. Branching on `is_error()` is clearer than relying on the exception path.

## `load_result`

`load()` and `load_file()` return a `load_result` that represents a compiled chunk. The error-checking shape is identical to `function_result`:

```cpp
auto chunk = s.load("return math.pi");
if (chunk.is_error()) {
    std::cerr << chunk.error();
    return;
}
double pi = chunk();                 // execute the chunk
```

Keeping the compiled chunk around lets you invoke the same script repeatedly without re-parsing.

## Stack Cleanup

Errors never leak stack slots:

- `stack_reference` and `registry_reference` destructors run during unwind and release their slot/registry key.
- `function_result` takes ownership of the return-value slice on the stack and pops it on destruction.
- `function_result::error()` reads the error value from the slice it owns; the slice is released when the result goes out of scope.

Do not mix raw `lua_pop` with uLua reference types on the same slot — the RAII wrapper is already tracking it.

## Summary

- Use `ulua::error`, `arg_error`, `type_error` inside C callbacks.
- Check `function_result::is_error()` before extracting, or rely on implicit conversion when failure should abort.
- Watch for the `<cassert>` macro collision on `.assert()`.
- RAII references and `function_result` clean up the stack on any unwind path.
