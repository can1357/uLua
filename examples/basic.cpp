#include <ulua.hpp>

#include <iostream>

int main() {
	ulua::state lua;
	lua.open_libraries(ulua::lib::base, ulua::lib::math, ulua::lib::string, ulua::lib::table);

	lua["sum"] = [](int a, int b) { return a + b; };

	auto result = lua.script("return sum(20, 22)", "basic-example");
	if (!result) {
		std::cerr << result.error() << '\n';
		return 1;
	}

	std::cout << "Lua says: " << result.as<int>() << '\n';
	return 0;
}
