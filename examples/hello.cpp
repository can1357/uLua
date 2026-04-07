#include <ulua.hpp>

#include <cstdio>
#include <string>

int main() {
	ulua::state L;
	L.open_libraries(ulua::lib::base, ulua::lib::string);

	L["greet"] = [](std::string name) { return "Hello, " + name + "!"; };

	if (auto r = L.script("print(greet('world'))"); !r) {
		std::fprintf(stderr, "script error: %s\n", r.error().c_str());
		return 1;
	}

	L["x"] = 42;

	if (auto r = L.script("print('x = ' .. tostring(x))"); !r) {
		std::fprintf(stderr, "script error: %s\n", r.error().c_str());
		return 1;
	}

	return 0;
}
