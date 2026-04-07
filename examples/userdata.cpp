// ulua userdata example: bind a small Vec2 type with fields, methods and operators.
#include <ulua.hpp>
#include <cstdio>
#include <cmath>
#include <string>
#include <tuple>

struct Vec2 {
	double x = 0;
	double y = 0;

	double length() const { return std::sqrt(x * x + y * y); }
	std::string to_string() const { return "Vec2(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }

	Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
	Vec2 operator*(double s) const { return {x * s, y * s}; }
	Vec2 operator-() const { return {-x, -y}; }
	bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
};

namespace ulua {
	template<> struct user_traits<Vec2> {
		static constexpr const char* name = "Vec2";
		static constexpr auto fields =
			std::make_tuple(ulua::member<&Vec2::x>("x"), ulua::member<&Vec2::y>("y"), ulua::member<&Vec2::length>("length"));
	};
}

int main() {
	ulua::state L;
	L.open_libraries(ulua::lib::base, ulua::lib::string, ulua::lib::math);

	// Constructor exposed as a global callable.
	L["Vec2"] = [](double x, double y) { return Vec2{x, y}; };

	constexpr const char* src = R"lua(
        local a = Vec2(3, 4)
        local b = Vec2(1, 2)

        print('a       = ' .. tostring(a))
        print('b       = ' .. tostring(b))
        print('a.x     = ' .. a.x)
        print('a.y     = ' .. a.y)
        print('|a|     = ' .. a:length())
        print('a + b   = ' .. tostring(a + b))
        print('a * 2   = ' .. tostring(a * 2))
        print('-a      = ' .. tostring(-a))
        print('a == a  = ' .. tostring(a == a))
    )lua";

	if (auto r = L.script(src); !r) {
		std::fprintf(stderr, "script error: %s\n", r.error().c_str());
		return 1;
	}
	return 0;
}
