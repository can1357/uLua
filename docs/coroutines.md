# Coroutines

`ulua::coroutine` extends `state_view`. A coroutine is simply a Lua thread
(`lua_State*` created by `lua_newthread`) paired with a registry anchor so
the GC does not collect the thread while C++ still holds it.

See also: [state.md](state.md), [functions.md](functions.md).

## Creating

Any pushable value can become the body of a coroutine:

```cpp
ulua::state s;
s.open_libraries(ulua::lib::base);

s.script(R"(
    function counter(start, step)
        local i = start
        while true do
            coroutine.yield(i)
            i = i + step
        end
    end
)");

ulua::function fn = s["counter"];
auto co = ulua::coroutine::create(fn);
```

`coroutine::create` allocates a new thread, pushes the target function onto
it, and returns a `coroutine` wrapper holding a registry reference to the
thread.

## Resuming

`resume` returns a Lua 5.1 status code directly:

```cpp
int status = co.resume(10, 2);   // starts the body with (start=10, step=2)
// status == LUA_YIELD (1) → yielded; call co.resume() again to continue.
// status == 0            → function returned; coroutine is dead.
// anything else          → runtime error; pull the error off the top slot.

while (status == LUA_YIELD) {
    int value = ulua::stack::get<int>(co.state(), -1);
    ulua::stack::pop_n(co.state(), 1);
    std::cout << value << '\n';
    if (value >= 20) break;
    status = co.resume();
}
```

`co.status()` returns the same code set without advancing the coroutine.
`ulua::coroutine::running(L)` reports whether `L` is a non-main thread —
useful inside a C function that needs to decide whether yielding is legal.

## Yielding from a C function

A C function can yield only when it is being executed on a coroutine
thread. Use the static form:

```cpp
static int gen_next(lua_State* L) {
    static int i = 0;
    // Yield i back to the Lua-side resume() caller.
    return ulua::coroutine::resume(L, i++);
}

s["gen_next"] = ulua::cfunction_t{&gen_next};
s.script(R"(
    local co = coroutine.create(function()
        for _ = 1, 3 do print(gen_next()) end
    end)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
)");
```

The static `coroutine::resume(L, results...)` pushes the results and calls
`lua_yield(L, nresults)`. Control returns to whichever Lua-side `resume`
invoked the C function. Calling it from the main thread is an error.

## Notes

- This API targets Lua 5.1's `lua_resume(L, nargs)` signature. LuaJIT
  preserves it, so the same code works on both.
- The coroutine's stack is independent of the main thread's stack. When
  reading yielded values, operate on `co.state()`, not the parent state.
- Destroying the `coroutine` object drops its registry anchor, making the
  Lua thread eligible for collection on the next GC cycle.
