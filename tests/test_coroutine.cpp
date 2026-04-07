#include <doctest/doctest.h>
#include <ulua.hpp>

TEST_CASE("create and resume coroutine") {
	ulua::state L;
	luaL_openlibs(L);

	L.script(R"(
        function gen()
            coroutine.yield(1)
            coroutine.yield(2)
            return 3
        end
    )");

	ulua::function f = L["gen"];
	ulua::coroutine co = ulua::coroutine::create(f);

	int status = co.resume();
	CHECK(status == LUA_YIELD);
	CHECK(ulua::stack::get<int>(co, 1) == 1);
	ulua::stack::set_top(co, 0);

	status = co.resume();
	CHECK(status == LUA_YIELD);
	CHECK(ulua::stack::get<int>(co, 1) == 2);
	ulua::stack::set_top(co, 0);

	status = co.resume();
	CHECK(status == 0);
	CHECK(ulua::stack::get<int>(co, 1) == 3);
	ulua::stack::set_top(co, 0);
}

TEST_CASE("coroutine bidirectional yield") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	L.script(R"(
        function bidir(seed)
            local got = coroutine.yield(seed + 1)
            return seed + got
        end
    )");
	ulua::function f = L["bidir"];

	CHECK_FALSE(ulua::coroutine::running(L));
	auto co = ulua::coroutine::create(f);
	CHECK(ulua::coroutine::running(co));

	// Resume passes 41 in, expects 42 yielded out.
	CHECK(co.resume(41) == LUA_YIELD);
	CHECK(ulua::stack::pop<int>(co) == 42);

	// Resume passes 1 in, expects 42 returned out.
	CHECK(co.resume(1) == 0);
	CHECK(ulua::stack::pop<int>(co) == 42);
}
