#include <ulua.hpp>

#include <iostream>
#include <string>
#include <string_view>
#include <tuple>

namespace {

	struct vec2 {
		int x = 0;
		int y = 0;

		int sum() const { return x + y; }

		void shift(int dx, int dy) {
			x += dx;
			y += dy;
		}

		std::string to_string() const { return "vec2(" + std::to_string(x) + "," + std::to_string(y) + ")"; }

		struct lua_traits {
			static constexpr std::string_view name = "Vec2";
			static constexpr auto fields = std::make_tuple(
				ulua::member<&vec2::x>("x"), ulua::member<&vec2::y>("y"), ulua::member<&vec2::y>("y_ro", ulua::readonly_t{}),
				ulua::member<&vec2::sum>("sum"), ulua::member<&vec2::shift>("shift"), ulua::static_member("axis_count", 2));
		};
	};

	int fail(const std::string& message) {
		std::cerr << message << '\n';
		return 1;
	}

	int check_userdata_behavior(ulua::state_view lua) {
		lua["vec"] = vec2{4, 5};

		auto vec_result = lua.script(R"(
    assert(vec.x == 4 and vec.y == 5)
    assert(vec.y_ro == 5)
    assert(vec:sum() == 9)
    assert(vec.axis_count == 2)
    vec:shift(10, 20)
    return vec.x, vec.y, tostring(vec)
  )",
		                             "advanced-userdata");
		if (!vec_result) { return fail("userdata script failed: " + vec_result.error()); }

		if (vec_result.as<int>(0) != 14 || vec_result.as<int>(1) != 25) { return fail("userdata fields did not update through Lua"); }

		if (vec_result.as<std::string>(2) != "vec2(14,25)") { return fail("userdata tostring metamethod returned an unexpected value"); }

		auto readonly_result = lua.script("vec.y_ro = 0", "advanced-readonly");
		if (readonly_result) { return fail("readonly userdata field write unexpectedly succeeded"); }
		if (readonly_result.error().find("setting read-only property") == std::string::npos) {
			return fail("readonly userdata field write returned the wrong error: " + readonly_result.error());
		}

		return 0;
	}

	int check_frozen_table(ulua::state_view lua) {
		auto frozen = lua.make_table();
		frozen["answer"] = 42;
		ulua::freeze_table(frozen);
		lua["frozen"] = frozen;

		auto read_result = lua.script("return frozen.answer", "advanced-frozen-read");
		if (!read_result) { return fail("frozen table read failed: " + read_result.error()); }
		if (read_result.as<int>() != 42) { return fail("frozen table lost its original value"); }

		auto write_result = lua.script("frozen.answer = 7", "advanced-frozen-write");
		if (write_result) { return fail("frozen table write unexpectedly succeeded"); }
		if (write_result.error().find("cannot modify immutable table") == std::string::npos) {
			return fail("frozen table write returned the wrong error");
		}
		auto append_result = lua.script("frozen.extra = 7", "advanced-frozen-append");
		if (append_result) { return fail("frozen table accepted a new key unexpectedly"); }
		if (append_result.error().find("cannot modify immutable table") == std::string::npos) {
			return fail("frozen table append returned the wrong error");
		}

		return 0;
	}

	int check_coroutines(ulua::state_view lua) {
		if (ulua::coroutine::running(lua)) { return fail("main thread was reported as a coroutine"); }

		auto coroutine_factory = lua.script(R"(
    return function(seed)
      local incoming = coroutine.yield(seed + 1)
      return seed + incoming
    end
  )",
		                                    "advanced-coroutine-factory");
		if (!coroutine_factory) { return fail("coroutine factory script failed: " + coroutine_factory.error()); }

		auto thread = ulua::coroutine::create(coroutine_factory.get_ref());
		if (!ulua::coroutine::running(thread)) { return fail("new coroutine was not reported as running"); }

		if (thread.resume(41) != LUA_YIELD) { return fail("coroutine did not yield on first resume"); }
		if (ulua::stack::pop<int>(thread) != 42) { return fail("coroutine yielded an unexpected value"); }

		if (thread.resume(1) != 0) { return fail("coroutine did not finish cleanly on second resume"); }
		if (ulua::stack::pop<int>(thread) != 42) { return fail("coroutine returned an unexpected final value"); }

		return 0;
	}

} // namespace

int main() {
	ulua::state lua;
	if (!lua) { return fail("failed to allocate Lua state"); }

	lua.open_libraries(ulua::lib::base, ulua::lib::math, ulua::lib::string, ulua::lib::table);

	if (int rc = check_userdata_behavior(lua); rc != 0) { return rc; }
	if (int rc = check_frozen_table(lua); rc != 0) { return rc; }
	if (int rc = check_coroutines(lua); rc != 0) { return rc; }

	return 0;
}
