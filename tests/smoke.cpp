#include <ulua.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
	int fail(const std::string& message) {
		std::cerr << message << '\n';
		return 1;
	}
} // namespace

int main() {
	ulua::state lua;
	if (!lua) { return fail("failed to allocate Lua state"); }

	lua.open_libraries(ulua::lib::base, ulua::lib::math, ulua::lib::string, ulua::lib::table);

	lua["sum"] = [](int a, int b) { return a + b; };

	auto inline_result = lua.script("return sum(19, 23)", "inline-smoke");
	if (!inline_result) { return fail("inline script failed: " + inline_result.error()); }
	if (inline_result.as<int>() != 42) { return fail("inline script returned an unexpected value"); }

	ulua::environment env{lua, ulua::create{}, lua.globals()};
	env["base"] = 40;

	auto env_result = lua.script("return base + 2", env, "environment-smoke");
	if (!env_result) { return fail("environment script failed: " + env_result.error()); }
	if (env_result.as<int>() != 42) { return fail("environment script returned an unexpected value"); }

	const auto script_path = std::filesystem::temp_directory_path() / "ulua-smoke.lua";
	{
		std::ofstream out(script_path);
		out << "return base + 2\n";
	}

	const auto script_file = script_path.string();
	auto file_result = lua.script_file(script_file.c_str(), env);
	std::filesystem::remove(script_path);

	if (!file_result) { return fail("script_file failed: " + file_result.error()); }
	if (file_result.as<int>() != 42) { return fail("script_file returned an unexpected value"); }

	return 0;
}
