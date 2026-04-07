// Real LuaJIT private-header FFI / cdata coverage.
//
// Compiled only when ULUA_HAS_LUAJIT_PRIVATE_HEADERS is detected by tests/CMakeLists.txt.
// Validates that ulua's ctype_t-backed userdata path:
//   - Lands on the Lua stack as cdata
//   - Round-trips through ulua::cdata_value
//   - Resolves the typedef'd ctype name correctly (regression test for the
//     ffi::typeid_of mask bug that left cdata with ctypeid=0 and made GC
//     spin forever in lj_cdata_free -> ctype_raw)
//   - Honors registered fields, methods, and arithmetic via ffi.metatype
//   - Survives explicit lua_gc() and lua_close() teardown
//
// Plus a sanity check that ffi::cdef + a hand-rolled ffi.new() chain still
// behaves end-to-end inside the same state.
#include <ulua.hpp>

#include <cmath>
#include <cstdio>
#include <string>
#include <tuple>

#if !ULUA_JIT
	#error "tests/ffi.cpp requires LuaJIT private headers"
#endif

namespace {

	struct FfiVec2 {
		double x = 0;
		double y = 0;

		FfiVec2() = default;
		FfiVec2(double x, double y) : x(x), y(y) {}

		double length() const { return std::sqrt(x * x + y * y); }

		FfiVec2 operator+(const FfiVec2& o) const { return {x + o.x, y + o.y}; }
		FfiVec2 operator-(const FfiVec2& o) const { return {x - o.x, y - o.y}; }
		bool operator==(const FfiVec2& o) const { return x == o.x && y == o.y; }
	};

} // namespace

namespace ulua {
	template<> struct user_traits<FfiVec2> : ctype_t {
		static constexpr const char* name = "ffi_vec2_t";

		// Typedef'd struct on purpose: this is the exact pattern that used to
		// make ffi::typeid_of return 0 because the lookup mask omitted CT_TYPEDEF.
		static constexpr auto cdef = R"(
        typedef struct {
            double x;
            double y;
        } ffi_vec2_t;
    )";

		static constexpr auto fields = std::make_tuple(member<&FfiVec2::length>("length"));
	};
}

namespace {

	int fail(const std::string& message) {
		std::fprintf(stderr, "%s\n", message.c_str());
		return 1;
	}

	int check_cdata_stack_roundtrip(ulua::state_view lua) {
		ulua::stack::push(lua, FfiVec2{3.0, 4.0});
		const int slot = ulua::stack::top(lua);
		if (ulua::stack::type(lua, slot) != ulua::value_type::cdata) return fail("FFI-backed userdata did not land on the stack as cdata");

		int cdata_slot = slot;
		auto cdata = ulua::stack::get<ulua::cdata_value>(lua, cdata_slot);
		if (!cdata.pointer) return fail("failed to read cdata_value from the stack");

		int ref_slot = slot;
		FfiVec2& ref = ulua::stack::get<FfiVec2&>(lua, ref_slot);
		if (ref.x != 3.0 || ref.y != 4.0) return fail("failed to recover the original C++ value from cdata");

		// Push the cdata_value again and confirm it round-trips back to the same payload.
		ulua::stack::push(lua, cdata);
		const int copy_slot = ulua::stack::top(lua);
		if (ulua::stack::type(lua, copy_slot) != ulua::value_type::cdata)
			return fail("pushing cdata_value did not recreate cdata on the stack");

		int copy_ref_slot = copy_slot;
		FfiVec2& copy = ulua::stack::get<FfiVec2&>(lua, copy_ref_slot);
		if (copy.x != 3.0 || copy.y != 4.0) return fail("cdata_value push/get roundtrip changed the stored payload");

		ulua::stack::pop_n(lua, 2);
		return 0;
	}

	int check_ffi_metatype_behavior(ulua::state_view lua) {
		// Bind a C++ helper that consumes a cdata-backed FfiVec2 by reference.
		lua["component_sum"] = [](const FfiVec2& value) { return value.x + value.y; };
		lua["a"] = FfiVec2{3.0, 4.0};
		lua["b"] = FfiVec2{1.0, 2.0};

		// type(a) must be 'cdata' under the real LuaJIT path.
		auto type_result = lua.script("return type(a), component_sum(a), component_sum(b)", "ffi-type-a");
		if (!type_result) return fail("type(a) probe failed: " + type_result.error());
		if (type_result.as<std::string>(0) != "cdata") return fail("Lua did not observe FFI-backed userdata as cdata");
		if (type_result.as<double>(1) != 7.0 || type_result.as<double>(2) != 3.0)
			return fail("C++ could not read the cdata payload via reference");

		// ffi.sizeof must agree with the C++ struct size, which proves the typedef
		// resolved to a real ctype with the right size (and that ulua's typeid
		// lookup did not regress to 0).
		auto sizeof_result = lua.script("return ffi.sizeof('ffi_vec2_t')", "ffi-sizeof");
		if (!sizeof_result) return fail("ffi.sizeof probe failed: " + sizeof_result.error());
		if (sizeof_result.as<int>(0) != static_cast<int>(sizeof(FfiVec2))) return fail("ffi.sizeof reported an unexpected ctype size");

		// Native LuaJIT struct field access on cdata.
		auto field_result = lua.script("return a.x, a.y, b.x, b.y", "ffi-fields");
		if (!field_result) return fail("raw cdata field access failed: " + field_result.error());
		if (field_result.as<double>(0) != 3.0 || field_result.as<double>(1) != 4.0 || field_result.as<double>(2) != 1.0 ||
		    field_result.as<double>(3) != 2.0)
			return fail("raw cdata field access returned wrong values");

		// Method call goes through ulua's generated __index dispatch on the metatype.
		auto method_result = lua.script("return a:length(), b:length()", "ffi-method");
		if (!method_result) return fail("cdata method dispatch failed: " + method_result.error());
		if (method_result.as<double>(0) != 5.0) return fail("a:length() returned wrong value");
		if (std::abs(method_result.as<double>(1) - std::sqrt(5.0)) > 1e-12) return fail("b:length() returned wrong value");

		// Arithmetic goes through ulua's generated __add / __sub on the metatype.
		auto arith_result = lua.script("local s = a + b\n"
		                               "local d = a - b\n"
		                               "return s.x, s.y, d.x, d.y",
		                               "ffi-arith");
		if (!arith_result) return fail("cdata arithmetic failed: " + arith_result.error());
		if (arith_result.as<double>(0) != 4.0 || arith_result.as<double>(1) != 6.0) return fail("a + b returned wrong values");
		if (arith_result.as<double>(2) != 2.0 || arith_result.as<double>(3) != 2.0) return fail("a - b returned wrong values");

		// Equality must reach the generated __eq metamethod (cdata != cdata by default).
		auto eq_result = lua.script("local c = a + b - b\n"
		                            "return c == a, c == b",
		                            "ffi-eq");
		if (!eq_result) return fail("cdata equality failed: " + eq_result.error());
		if (!eq_result.as<bool>(0)) return fail("(a + b - b) == a should be true");
		if (eq_result.as<bool>(1)) return fail("(a + b - b) == b should be false");

		// Drop globals and force a full GC cycle. Before the typeid_of fix this
		// hung lj_gc_freeall in an infinite ctype_raw loop because every cdata had
		// ctypeid=0 (a self-referencing CT_ATTRIB).
		lua["a"] = ulua::nil;
		lua["b"] = ulua::nil;
		lua["component_sum"] = ulua::nil;
		lua.collect_garbage();

		return 0;
	}

	int check_direct_ffi_api(ulua::state_view lua) {
		auto cdef_result = ulua::ffi::cdef(lua, "typedef struct { int value; } ffi_counter_t;");
		if (!cdef_result) return fail("ffi::cdef failed: " + cdef_result.error());

		auto result = lua.script(R"(
        local counter = ffi.new("ffi_counter_t")
        counter.value = 41
        return type(counter), counter.value + 1
    )",
		                         "ffi-direct-api");
		if (!result) return fail("direct ffi API script failed: " + result.error());

		if (result.as<std::string>(0) != "cdata") return fail("ffi::cdef did not register a cdata type visible to Lua");
		if (result.as<int>(1) != 42) return fail("ffi-created cdata did not behave as expected");

		return 0;
	}

} // namespace

int main() {
	ulua::state lua;
	if (!lua) return fail("failed to allocate Lua state");

	lua.open_libraries(ulua::lib::base, ulua::lib::math, ulua::lib::ffi);

	if (int rc = check_cdata_stack_roundtrip(lua); rc != 0) return rc;
	if (int rc = check_ffi_metatype_behavior(lua); rc != 0) return rc;
	if (int rc = check_direct_ffi_api(lua); rc != 0) return rc;

	return 0;
}
