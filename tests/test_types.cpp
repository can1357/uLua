#include <doctest/doctest.h>
#include <ulua.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <variant>

namespace { enum class color : int { red = 1, green = 2, blue = 3 }; }

TEST_CASE("integer round-trip") {
	ulua::state L;

	SUBCASE("int") {
		ulua::stack::push<int>(L, 42);
		CHECK(ulua::stack::pop<int>(L) == 42);
	}
	SUBCASE("int64_t") {
		ulua::stack::push<int64_t>(L, int64_t(1234567));
		CHECK(ulua::stack::pop<int64_t>(L) == 1234567);
	}
	SUBCASE("uint32_t") {
		ulua::stack::push<uint32_t>(L, uint32_t(4242));
		CHECK(ulua::stack::pop<uint32_t>(L) == 4242u);
	}
}

TEST_CASE("floating-point round-trip") {
	ulua::state L;
	ulua::stack::push<double>(L, 3.14);
	CHECK(ulua::stack::pop<double>(L) == doctest::Approx(3.14));
}

TEST_CASE("bool round-trip") {
	ulua::state L;
	ulua::stack::push(L, true);
	CHECK(ulua::stack::pop<bool>(L) == true);
	ulua::stack::push(L, false);
	CHECK(ulua::stack::pop<bool>(L) == false);
}

TEST_CASE("string round-trip") {
	ulua::state L;

	SUBCASE("string_view") {
		ulua::stack::push(L, std::string_view("abc"));
		auto sv = ulua::stack::pop<std::string_view>(L);
		CHECK(sv == "abc");
	}
	SUBCASE("std::string") {
		ulua::stack::push(L, std::string("xyz"));
		auto s = ulua::stack::pop<std::string>(L);
		CHECK(s == "xyz");
	}
}

TEST_CASE("nil round-trip") {
	ulua::state L;
	ulua::stack::push(L, ulua::nil);
	int slot = ulua::stack::top(L);
	CHECK(ulua::stack::check<ulua::nil_t>(L, slot));
	ulua::stack::pop_n(L, 1);
}

TEST_CASE("optional round-trip") {
	ulua::state L;

	SUBCASE("with value") {
		ulua::stack::push<std::optional<int>>(L, std::optional<int>{42});
		auto v = ulua::stack::pop<std::optional<int>>(L);
		REQUIRE(v.has_value());
		CHECK(*v == 42);
	}
	SUBCASE("nullopt") {
		ulua::stack::push<std::optional<int>>(L, std::nullopt);
		auto v = ulua::stack::pop<std::optional<int>>(L);
		CHECK_FALSE(v.has_value());
	}
}

TEST_CASE("tuple round-trip") {
	ulua::state L;
	using tup = std::tuple<int, double, std::string>;
	ulua::stack::push<tup>(L, tup{7, 2.5, std::string("hi")});
	int slot = 1;
	auto got = ulua::type_traits<tup>::get(L, slot);
	CHECK(std::get<0>(got) == 7);
	CHECK(std::get<1>(got) == doctest::Approx(2.5));
	CHECK(std::get<2>(got) == "hi");
	ulua::stack::pop_n(L, 3);
}

TEST_CASE("variant round-trip") {
	ulua::state L;
	using var = std::variant<int, std::string>;
	ulua::stack::push<var>(L, var{123});
	auto v = ulua::stack::pop<var>(L);
	REQUIRE(v.index() == 0);
	CHECK(std::get<0>(v) == 123);
}

TEST_CASE("enum round-trip") {
	ulua::state L;
	ulua::stack::push<color>(L, color::green);
	auto c = ulua::stack::pop<color>(L);
	CHECK(c == color::green);
}
