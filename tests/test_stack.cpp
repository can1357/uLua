#include <doctest/doctest.h>
#include <ulua.hpp>

TEST_CASE("push and pop primitives") {
	ulua::state L;

	SUBCASE("integer") {
		ulua::stack::push(L, 42);
		CHECK(ulua::stack::pop<int>(L) == 42);
	}

	SUBCASE("double") {
		ulua::stack::push(L, 3.14);
		CHECK(ulua::stack::pop<double>(L) == doctest::Approx(3.14));
	}

	SUBCASE("bool") {
		ulua::stack::push(L, true);
		CHECK(ulua::stack::pop<bool>(L) == true);
	}

	SUBCASE("string") {
		ulua::stack::push(L, std::string_view("hello"));
		auto s = ulua::stack::pop<std::string>(L);
		CHECK(s == "hello");
	}
}

TEST_CASE("stack top tracking") {
	ulua::state L;
	CHECK(ulua::stack::top(L) == 0);

	ulua::stack::push(L, 1);
	ulua::stack::push(L, 2);
	ulua::stack::push(L, 3);
	CHECK(ulua::stack::top(L) == 3);

	ulua::stack::pop_n(L, 2);
	CHECK(ulua::stack::top(L) == 1);
}

TEST_CASE("abs and rel conversion") {
	ulua::state L;
	ulua::stack::push(L, 10);
	ulua::stack::push(L, 20);
	ulua::stack::push(L, 30);

	CHECK(ulua::stack::abs(L, -1) == 3);
	CHECK(ulua::stack::abs(L, -2) == 2);
	CHECK(ulua::stack::abs(L, -3) == 1);

	CHECK(ulua::stack::rel(L, 1) == -3);
	CHECK(ulua::stack::rel(L, 2) == -2);
	CHECK(ulua::stack::rel(L, 3) == -1);

	CHECK(ulua::stack::abs(L, 2) == 2);
}
