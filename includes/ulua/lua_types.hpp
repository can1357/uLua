#pragma once
#include <lua.hpp>
#include <type_traits>
#include <string_view>
#include "common.hpp"

namespace ulua
{	
	enum class value_type
	{
		nil =            LUA_TNIL,
		boolean =        LUA_TBOOLEAN,
		light_userdata = LUA_TLIGHTUSERDATA,
		number =         LUA_TNUMBER,
		string =         LUA_TSTRING,
		table =          LUA_TTABLE,
		function =       LUA_TFUNCTION,
		userdata =       LUA_TUSERDATA,
		thread =         LUA_TTHREAD,
	};
	inline const char* type_name( value_type type ) { return lua_typename( nullptr, ( int ) type ); }

	// Metatable fields.
	//
	enum class meta : uint8_t
	{
		metatable, newindex,  index, gc,
		tostring,  name,      len,   ipairs,
		pairs,     unm,       add,   sub,
		mul,       div,       idiv,  mod,
		pow,       concat,    eq,    lt,
		le
	};
	inline constexpr const char* metafield_name( meta field ) 
	{
		constexpr const char* arr[] = {
			"__metatable", "__newindex", "__index",     "__gc",
			"__tostring",  "__name",     "__len",       "__ipairs",
			"__pairs",     "__unm",      "__add",       "__sub",
			"__mul",       "__div",      "__idiv",      "__mod",
			"__pow",       "__concat",   "__eq",        "__lt",
			"__le"
		};
		return arr[ ( size_t ) field ];
	}

	// Primitive types.
	//
	struct nil_t { struct tag {}; constexpr explicit nil_t( tag ) {} };
	inline constexpr nil_t nil{ nil_t::tag{} };

	using cfunction_t = lua_CFunction;
	
	struct light_userdata
	{
		void* pointer;
		operator void*() const { return pointer; }
	};

	// Primitive type traits.
	//
	template<typename T>
	struct type_traits;
	template<typename T> struct type_traits<T&> : type_traits<T> {};
	template<typename T> struct type_traits<T&&> : type_traits<T> {};
	template<typename T> struct type_traits<const T> : type_traits<T> {};
	template<typename T, size_t N> struct type_traits<T[N]> : type_traits<T*> {};
	template<typename T, size_t N> struct type_traits<T(&)[N]> : type_traits<T*> {};
	template<typename T, size_t N> struct type_traits<T(*)[N]> : type_traits<T*> {};

	template<typename T> requires std::is_integral_v<T>
	struct type_traits<T>
	{
		inline static void push( lua_State* L, T value )
		{
			lua_pushinteger( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isnumber( L, idx );
		}
		inline static T get( lua_State* L, int idx )
		{
			return ( T ) luaL_checkinteger( L, idx );
		}
	};
	template<typename T> requires std::is_floating_point_v<T>
	struct type_traits<T>
	{
		inline static void push( lua_State* L, T value )
		{
			lua_pushnumber( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isnumber( L, idx );
		}
		inline static T get( lua_State* L, int idx )
		{
			return ( T ) luaL_checknumber( L, idx );
		}
	};
	template<>
	struct type_traits<std::string_view>
	{
		inline static void push( lua_State* L, std::string_view value )
		{
			lua_pushlstring( L, value.data(), value.length() );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isstring( L, idx );
		}
		inline static std::string_view get( lua_State* L, int idx )
		{
			size_t length;
			const char* data = luaL_checklstring( L, idx, &length );
			return { data, data + length };
		}
	};
	template<>
	struct type_traits<const char*> : type_traits<std::string_view>
	{
		inline static const char* get( lua_State* L, int idx ) { return luaL_checkstring( L, idx ); }
	};
	template<>
	struct type_traits<std::string> : type_traits<std::string_view>
	{
		inline static std::string get( lua_State* L, int idx ) { return std::string{ type_traits<std::string_view>::get( L, idx ) }; }
	};
	template<>
	struct type_traits<nil_t>
	{
		inline static void push( lua_State* L, nil_t )
		{
			lua_pushnil( L );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isnil( L, idx );
		}
		inline static nil_t get( lua_State*, int )
		{
			return nil;
		}
	};
	template<>
	struct type_traits<bool>
	{
		inline static void push( lua_State* L, bool value )
		{
			lua_pushboolean( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isboolean( L, idx );
		}
		inline static bool get( lua_State* L, int idx )
		{
			return lua_toboolean( L, idx );
		}
	};
	template<>
	struct type_traits<cfunction_t>
	{
		inline static void push( lua_State* L, cfunction_t value )
		{
			lua_pushcfunction( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_iscfunction( L, idx );
		}
		inline static cfunction_t get( lua_State* L, int idx )
		{
			return lua_tocfunction( L, idx );
		}
	};
	template<>
	struct type_traits<light_userdata>
	{
		inline static void push( lua_State* L, light_userdata value )
		{
			lua_pushlightuserdata( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_islightuserdata( L, idx );
		}
		inline static light_userdata get( lua_State* L, int idx )
		{
			return { lua_touserdata( L, idx ) };
		}
	};
};