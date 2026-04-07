#include <doctest/doctest.h>
#include <ulua.hpp>

#include <tuple>

TEST_CASE("call lua function") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);
	L.script("function add(a, b) return a + b end");

	ulua::function add = L["add"];
	ulua::function_result r = add(3, 4);
	REQUIRE(r.is_success());
	CHECK(r.as<int>() == 7);
}

TEST_CASE("pcall error") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);
	L.script("function boom() error('bad') end");

	ulua::function f = L["boom"];
	ulua::function_result r = f();
	CHECK(r.is_error());
	CHECK(!r.error().empty());
}

TEST_CASE("bind C++ lambda") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	L["mul"] = [](int a, int b) { return a * b; };

	auto r = L.script("return mul(6, 7)");
	REQUIRE(r.is_success());
	CHECK(r.as<int>() == 42);
}

TEST_CASE("multi-return") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);
	L.script("function three() return 1, 2.5, 'x' end");

	ulua::function f = L["three"];
	ulua::function_result r = f();
	REQUIRE(r.is_success());
	REQUIRE(r.size() == 3);
	CHECK(r.as<int>(0) == 1);
	CHECK(r.as<double>(1) == doctest::Approx(2.5));
	CHECK(r.as<std::string>(2) == "x");
}
