#include <doctest/doctest.h>
#include <ulua.hpp>

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {
	struct Vec2 {
		double x, y;
		Vec2(double x = 0, double y = 0) : x(x), y(y) {}
		double length() const { return std::sqrt(x * x + y * y); }
		std::string to_string() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }
		bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
		Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
		Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
		Vec2 operator-() const { return {-x, -y}; }
		Vec2 operator*(double s) const { return {x * s, y * s}; }
	};

	struct ScalarBox {
		int64_t value = 0;

		ScalarBox() = default;
		explicit ScalarBox(int64_t value) : value(value) {}

		bool operator==(const ScalarBox&) const = default;
		ScalarBox operator-() const { return ScalarBox{-value}; }
		ScalarBox operator+(const ScalarBox& other) const { return ScalarBox{value + other.value}; }
		ScalarBox operator+(double other) const { return ScalarBox{value + static_cast<int64_t>(other)}; }
		ScalarBox operator-(const ScalarBox& other) const { return ScalarBox{value - other.value}; }
		ScalarBox operator-(double other) const { return ScalarBox{value - static_cast<int64_t>(other)}; }
		ScalarBox operator*(const ScalarBox& other) const { return ScalarBox{value * other.value}; }
		ScalarBox operator*(double other) const { return ScalarBox{value * static_cast<int64_t>(other)}; }
		double operator/(const ScalarBox& other) const { return static_cast<double>(value) / static_cast<double>(other.value); }
		double operator/(double other) const { return static_cast<double>(value) / other; }
		int64_t operator%(const ScalarBox& other) const { return value % other.value; }
		int64_t operator%(int64_t other) const { return value % other; }
	};

	inline ScalarBox operator+(double lhs, const ScalarBox& rhs) { return ScalarBox{static_cast<int64_t>(lhs) + rhs.value}; }

	inline ScalarBox operator*(double lhs, const ScalarBox& rhs) { return ScalarBox{static_cast<int64_t>(lhs) * rhs.value}; }

	inline int64_t pow(const ScalarBox& lhs, const ScalarBox& rhs) {
		int64_t result = 1;
		for (int64_t i = 0; i < rhs.value; ++i) { result *= lhs.value; }
		return result;
	}

	inline int64_t pow(const ScalarBox& lhs, double rhs) { return pow(lhs, ScalarBox{static_cast<int64_t>(rhs)}); }

	struct IntSequence {
		std::vector<int> values;

		size_t size() const { return values.size(); }
		auto begin() const { return values.begin(); }
		auto end() const { return values.end(); }
	};

	struct StringIntMap {
		std::map<std::string, int> values;

		size_t size() const { return values.size(); }
		auto begin() const { return values.begin(); }
		auto end() const { return values.end(); }
	};

	struct LifetimeProbe {
		inline static int destructed = 0;

		int value = 0;

		LifetimeProbe() = default;
		explicit LifetimeProbe(int value) : value(value) {}
		~LifetimeProbe() { ++destructed; }
	};

	template<typename T> ulua::userdata_wrapper<T>* get_global_userdata_wrapper(ulua::state& L, const char* name) {
		lua_getglobal(L, name);
		int slot = ulua::stack::top(L);
		auto raw = ulua::stack::get<ulua::userdata_value>(L, slot);
		ulua::stack::pop_n(L, 1);
		return std::launder((ulua::userdata_wrapper<T>*)raw.pointer);
	}

	template<typename T> void expose_metafield(ulua::state& L, const char* name, ulua::meta field) {
		ulua::userdata_metatable<T>::push(L);
		ulua::stack::get_field(L, ulua::stack::top(L), field);
		lua_setglobal(L, name);
		ulua::stack::pop_n(L, 1);
	}
} // namespace

namespace ulua {
	template<> struct user_traits<Vec2> {
		static constexpr const char* name = "Vec2";
		static constexpr auto fields = std::make_tuple(member<&Vec2::x>("x"), member<&Vec2::y>("y"), member<&Vec2::length>("length"));
		static constexpr std::tuple<> metatable = {};
	};

	template<> struct user_traits<ScalarBox> {
		static constexpr const char* name = "ScalarBox";
		static constexpr auto fields = std::make_tuple(member<&ScalarBox::value>("value"));
	};

	template<> struct user_traits<IntSequence> {
		static constexpr const char* name = "IntSequence";
	};

	template<> struct user_traits<StringIntMap> {
		static constexpr const char* name = "StringIntMap";
	};

	template<> struct user_traits<LifetimeProbe> {
		static constexpr const char* name = "LifetimeProbe";
		static constexpr auto fields = std::make_tuple(member<&LifetimeProbe::value>("value"));
	};
}

TEST_CASE("push and get userdata") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	ulua::stack::push(L, Vec2{3.0, 4.0});
	int slot = ulua::stack::top(L);
	Vec2& v = ulua::stack::get<Vec2&>(L, slot);
	CHECK(v.x == doctest::Approx(3.0));
	CHECK(v.y == doctest::Approx(4.0));
	CHECK(v.length() == doctest::Approx(5.0));
	ulua::stack::pop_n(L, 1);
}

TEST_CASE("userdata by pointer") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	Vec2 original{1.0, 2.0};
	ulua::stack::push(L, &original);
	int slot = ulua::stack::top(L);
	Vec2* p = ulua::stack::get<Vec2*>(L, slot);
	REQUIRE(p != nullptr);
	p->x = 10.0;
	CHECK(original.x == doctest::Approx(10.0));
	ulua::stack::pop_n(L, 1);
}

TEST_CASE("userdata metatable auto-generation") {
	ulua::state L;
	L.open_libraries(ulua::lib::base, ulua::lib::string);

	L["v"] = Vec2{3.0, 4.0};
	auto r = L.script("return tostring(v)");
	REQUIRE(r.is_success());
	auto s = r.as<std::string>();
	CHECK(s.find("3") != std::string::npos);
	CHECK(s.find("4") != std::string::npos);
}

TEST_CASE("userdata arithmetic metamethods") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	L["a"] = ScalarBox{10};
	L["b"] = ScalarBox{3};

	expose_metafield<ScalarBox>(L, "box_idiv", ulua::meta::idiv);

	auto r = L.script(R"(
        local sum = a + b
        local sum_num = a + 2
        local sum_left = 2 + a
        local diff = a - b
        local diff_num = a - 2
        local prod = a * b
        local prod_num = a * 2
        local prod_left = 2 * a
        local quotient = a / b
        local quotient_num = a / 2
        local floor_ud = box_idiv(a, b)
        local floor_num = box_idiv(a, 2)
        local modulo_ud = a % b
        local modulo_num = a % 4
        local power_ud = a ^ b
        local power_num = a ^ 3
        local neg = -a
        return sum.value, sum_num.value, sum_left.value,
               diff.value, diff_num.value,
               prod.value, prod_num.value, prod_left.value,
               quotient, quotient_num,
               floor_ud, floor_num,
               modulo_ud, modulo_num,
               power_ud, power_num,
               neg.value,
               a == a,
               (a .. b).value
    )");
	REQUIRE(r.is_success());
	CHECK(r.as<int64_t>(0) == 13);
	CHECK(r.as<int64_t>(1) == 12);
	CHECK(r.as<int64_t>(2) == 12);
	CHECK(r.as<int64_t>(3) == 7);
	CHECK(r.as<int64_t>(4) == 8);
	CHECK(r.as<int64_t>(5) == 30);
	CHECK(r.as<int64_t>(6) == 20);
	CHECK(r.as<int64_t>(7) == 20);
	CHECK(r.as<double>(8) == doctest::Approx(10.0 / 3.0));
	CHECK(r.as<double>(9) == doctest::Approx(5.0));
	CHECK(r.as<int64_t>(10) == 3);
	CHECK(r.as<int64_t>(11) == 5);
	CHECK(r.as<int64_t>(12) == 1);
	CHECK(r.as<int64_t>(13) == 2);
	CHECK(r.as<int64_t>(14) == 1000);
	CHECK(r.as<int64_t>(15) == 1000);
	CHECK(r.as<int64_t>(16) == -10);
	CHECK(r.as<bool>(17) == true);
	CHECK(r.as<int64_t>(18) == 13);
}

TEST_CASE("userdata pairs and ipairs behavior") {
	ulua::state L;
	L.open_libraries(ulua::lib::base, ulua::lib::table);

	L["seq"] = IntSequence{{4, 5, 6}};
	L["dict"] = StringIntMap{{{{"alpha", 1}, {"beta", 2}}}};

	expose_metafield<IntSequence>(L, "seq_pairs", ulua::meta::pairs);
	expose_metafield<IntSequence>(L, "seq_ipairs", ulua::meta::ipairs);
	expose_metafield<StringIntMap>(L, "dict_pairs", ulua::meta::pairs);

	auto r = L.script(R"(
        local seq_pairs_out = {}
        for i, v in seq_pairs(seq) do
            seq_pairs_out[#seq_pairs_out + 1] = tostring(i) .. ":" .. tostring(v)
        end

        local seq_ipairs_out = {}
        for i, v in seq_ipairs(seq) do
            seq_ipairs_out[#seq_ipairs_out + 1] = tostring(i) .. ":" .. tostring(v)
        end

        local dict_pairs_out = {}
        for k, v in dict_pairs(dict) do
            dict_pairs_out[#dict_pairs_out + 1] = k .. ":" .. tostring(v)
        end

        return table.concat(seq_pairs_out, ","),
               table.concat(seq_ipairs_out, ","),
               table.concat(dict_pairs_out, ",")
    )");
	REQUIRE(r.is_success());
	CHECK(r.as<std::string>(0) == "1:4,2:5,3:6");
	CHECK(r.as<std::string>(1) == "1:4,2:5,3:6");
	CHECK(r.as<std::string>(2) == "alpha:1,beta:2");
}

TEST_CASE("userdata gc respects value and pointer ownership") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	LifetimeProbe::destructed = 0;
	L["owned"] = LifetimeProbe{7};
	LifetimeProbe::destructed = 0;
	L["owned"] = ulua::nil;
	L.collect_garbage();
	CHECK(LifetimeProbe::destructed == 1);

	LifetimeProbe::destructed = 0;
	{
		LifetimeProbe external{9};
		L["borrowed"] = &external;
		L["borrowed"] = ulua::nil;
		L.collect_garbage();
		CHECK(LifetimeProbe::destructed == 0);
	}
	CHECK(LifetimeProbe::destructed == 1);
}

TEST_CASE("expired pointer-backed userdata fails safely") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	Vec2 first{1.0, 2.0};
	Vec2 second{3.0, 4.0};
	L["a"] = &first;
	L["b"] = &second;

	auto* wrapper = get_global_userdata_wrapper<Vec2>(L, "a");
	REQUIRE(wrapper != nullptr);
	wrapper->retire();

	auto r = L.script(R"(
        local ok, err = pcall(function() return a.x end)
        return ok, err, a == b, a < b, a <= b
    )");
	REQUIRE(r.is_success());
	CHECK(r.as<bool>(0) == false);
	CHECK(r.as<std::string>(1).find("expired") != std::string::npos);
	CHECK(r.as<bool>(2) == false);
	CHECK(r.as<bool>(3) == true);
	CHECK(r.as<bool>(4) == true);
}

// Opt-in via the lua_traits inner type (alternative to specializing
// ulua::user_traits<T>). Also exercises readonly_t and static_member.
namespace {
	struct Point {
		int x = 0;
		int y = 0;
		int sum() const { return x + y; }
		void shift(int dx, int dy) {
			x += dx;
			y += dy;
		}

		struct lua_traits {
			static constexpr std::string_view name = "Point";
			static constexpr auto fields = std::make_tuple(
				ulua::member<&Point::x>("x"), ulua::member<&Point::y>("y"), ulua::member<&Point::y>("y_ro", ulua::readonly_t{}),
				ulua::member<&Point::sum>("sum"), ulua::member<&Point::shift>("shift"), ulua::static_member("axis_count", 2));
		};
	};
} // namespace

TEST_CASE("lua_traits inner-type opt-in") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	L["p"] = Point{4, 5};
	auto r = L.script(R"(
        assert(p.x == 4 and p.y == 5)
        assert(p.y_ro == 5)
        assert(p:sum() == 9)
        assert(p.axis_count == 2)
        p:shift(10, 20)
        return p.x, p.y
    )");
	REQUIRE(r.is_success());
	CHECK(r.as<int>(0) == 14);
	CHECK(r.as<int>(1) == 25);
}

TEST_CASE("readonly field rejects writes") {
	ulua::state L;
	L.open_libraries(ulua::lib::base);

	L["p"] = Point{1, 2};
	auto r = L.script("p.y_ro = 99");
	CHECK(r.is_error());
	CHECK(r.error().find("setting read-only property") != std::string::npos);
}
