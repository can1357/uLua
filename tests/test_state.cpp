#include <doctest/doctest.h>
#include <ulua.hpp>

TEST_CASE("state creation and destruction") {
	ulua::state L;
	CHECK(static_cast<bool>(L));
	L.close();
	CHECK_FALSE(static_cast<bool>(L));
}

TEST_CASE("open libraries") {
	ulua::state L;
	L.open_libraries(ulua::lib::base, ulua::lib::string);
	auto r = L.script("return type(print)");
	REQUIRE(r.is_success());
	CHECK(std::string(r.as<std::string>()) == "function");
}

TEST_CASE("script execution") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);
	auto r = L.script("return 1 + 2");
	REQUIRE(r.is_success());
	CHECK(r.as<int>() == 3);
}

TEST_CASE("script error") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);
	auto r = L.script("this is not valid lua !!!");
	REQUIRE(r.is_error());
	CHECK(!r.error().empty());
}

TEST_CASE("load and call") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);
	auto chunk = L.load("return 42");
	REQUIRE(chunk.is_success());
	ulua::function_result r = std::move(chunk)();
	REQUIRE(r.is_success());
	CHECK(r.size() == 1);
	CHECK(r.as<int>() == 42);
}

TEST_CASE("globals access") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);
	L["x"] = 42;
	auto r = L.script("return x");
	REQUIRE(r.is_success());
	CHECK(r.as<int>() == 42);
}
