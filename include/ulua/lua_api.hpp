#pragma once
// ulua/lua_api.hpp -- Lua C API bridge and optional LuaJIT internal-header layer.
//
// Detects LuaJIT via <luajit.h>. When the LuaJIT private headers are available
// and ULUA_NO_ACCEL is not set, ulua enables its TValue/cdata fast paths.
// Otherwise it falls back to the portable Lua 5.1 API surface.
#include <lua.hpp>
#include <utility>

#ifndef __has_include
	#define __has_include(...) 0
#endif

#if __has_include(<luajit.h>)
extern "C" {
	#include <luajit.h>
};
#endif

// Determine whether we can use LuaJIT internal headers.
// ULUA_JIT gates all features that need LJ internals (TValue access, cdata/FFI,
// ctypes). ULUA_ACCEL is an alias; both require lj_obj.h et al. on the include path.
//
// To disable:  -DULUA_NO_ACCEL   (at compile time)
//
#if defined(LUAJIT_VERSION) && !defined(ULUA_NO_ACCEL) && __has_include(<lj_obj.h>) && \
    __has_include(<lj_state.h>) && \
    __has_include(<lj_cdata.h>) && \
    __has_include(<lj_cparse.h>) && \
    __has_include(<lj_tab.h>) && \
    __has_include(<lj_str.h>)
	#define ULUA_JIT   1
	#define ULUA_ACCEL 1
extern "C" {
	#include <lj_obj.h>
	#include <lj_state.h>
	#include <lj_cdata.h>
	#include <lj_cparse.h>
	#include <lj_tab.h>
	#include <lj_str.h>
};
#else
	#define ULUA_JIT   0
	#define ULUA_ACCEL 0
#endif

#if ULUA_JIT
namespace ulua::accel {
	inline TValue* ref(lua_State* L, int idx) {
		if (idx > 0) [[likely]] {
			TValue* o = L->base + (idx - 1);
			return o < L->top ? o : niltv(L);
		} else if (idx > LUA_REGISTRYINDEX) [[likely]] {
			return L->top + idx;
		} else if (idx == LUA_GLOBALSINDEX) {
			TValue* o = &G(L)->tmptv;
			settabV(L, o, tabref(L->env));
			return o;
		} else if (idx == LUA_REGISTRYINDEX) {
			return registry(L);
		} else {
			GCfunc* fn = curr_func(L);
			if (idx == LUA_ENVIRONINDEX) {
				TValue* o = &G(L)->tmptv;
				settabV(L, o, tabref(fn->c.env));
				return o;
			} else {
				idx = LUA_GLOBALSINDEX - idx;
				return idx <= fn->c.nupvalues ? &fn->c.upvalue[idx - 1] : niltv(L);
			}
		}
	}

	inline void xchg(lua_State* L, int a, int b) {
		TValue* p1 = a < 0 ? &L->top[a] : &L->base[a - 1];
		TValue* p2 = b < 0 ? &L->top[b] : &L->base[b - 1];
		std::swap(*p1, *p2);
	}

	inline void pop(lua_State* L, int i) { L->top -= i; }

	inline int top(lua_State* L) { return int(L->top - L->base); }
};
#endif