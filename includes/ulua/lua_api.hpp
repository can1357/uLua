#pragma once
#include <lua.hpp>

#ifndef __has_include
	#define __has_include(...) 0
#endif
#if __has_include(<luajit.h>)
extern "C" {
	#include <luajit.h>
};
#endif

#if defined(LUAJIT_VERSION)
	#define ULUA_JIT 1
	extern "C" {
		#include <lj_obj.h>
		#include <lj_state.h>
		#include <lj_cdata.h>
		#include <lj_cparse.h>
		#include <lj_tab.h>
		#include <lj_str.h>
	};
	#ifdef ULUA_NO_ACCEL
		#define ULUA_ACCEL 0
	#else
		#define ULUA_ACCEL 1
	#endif
#else
	#define ULUA_JIT 0
	#define ULUA_ACCEL 0
#endif

#if ULUA_JIT
	namespace ulua::accel
	{
		inline TValue* ref( lua_State* L, int idx )
		{
			if ( idx > 0 ) [[likely]]
			{
				TValue* o = L->base + ( idx - 1 );
				return o < L->top ? o : niltv( L );
			}
			else if ( idx > LUA_REGISTRYINDEX ) [[likely]]
			{
				return L->top + idx;
			}
			else if ( idx == LUA_GLOBALSINDEX )
			{
				TValue* o = &G( L )->tmptv;
				settabV( L, o, tabref( L->env ) );
				return o;
			}
			else if ( idx == LUA_REGISTRYINDEX )
			{
				return registry( L );
			}
			else
			{
				GCfunc* fn = curr_func( L );
				if ( idx == LUA_ENVIRONINDEX )
				{
					TValue* o = &G( L )->tmptv;
					settabV( L, o, tabref( fn->c.env ) );
					return o;
				}
				else
				{
					idx = LUA_GLOBALSINDEX - idx;
					return idx <= fn->c.nupvalues ? &fn->c.upvalue[ idx - 1 ] : niltv( L );
				}
			}
		}

		inline void xchg( lua_State* L, int a, int b )
		{
			TValue* p1 = a < 0 ? &L->top[ a ] : &L->base[ a - 1 ];
			TValue* p2 = b < 0 ? &L->top[ b ] : &L->base[ b - 1 ];
			std::swap( *p1, *p2 );
		}

		inline void pop( lua_State* L, int i )
		{
			L->top -= i;
		}

		inline int top( lua_State* L )
		{
			return int( L->top - L->base );
		}
	};
#endif