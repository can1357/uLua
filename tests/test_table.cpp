#include <doctest/doctest.h>
#include <ulua.hpp>

TEST_CASE("create and index table") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	auto t = L.make_table<ulua::table>();
	t["key"] = 99;
	int v = t["key"];
	CHECK(v == 99);
}

TEST_CASE("table iteration") {
	ulua::state L;
	L.open_libraries(ulua::lib::base, ulua::lib::string, ulua::lib::table);

	L.script("tbl = {a=1, b=2, c=3}");
	ulua::table t = L["tbl"];

	int count = 0;
	for ([[maybe_unused]] auto& [k, v] : t) { count++; }
	CHECK(count == 3);
}

TEST_CASE("nested table access") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	L.script("t = { a = {} }");
	L.script("t.a.b = 5");

	auto r = L.script("return t.a.b");
	REQUIRE(r.is_success());
	CHECK(r.as<int>() == 5);
}

TEST_CASE("freeze table") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	auto t = L.make_table<ulua::table>();
	t["key"] = 1;
	ulua::freeze_table(t);

	L["frozen"] = std::move(t);

	auto append = L.script("return pcall(function() frozen.new_key = 2 end)");
	REQUIRE(append.is_success());
	CHECK(append.as<bool>() == false);

	auto overwrite = L.script("return pcall(function() frozen.key = 2 end)");
	REQUIRE(overwrite.is_success());
	CHECK(overwrite.as<bool>() == false);
}
