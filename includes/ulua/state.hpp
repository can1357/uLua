#pragma once
#include "common.hpp"
#include "stack.hpp"
#include "reference.hpp"
#include "function.hpp"
#include "table.hpp"

namespace ulua
{
	// Libraries.
	//
	using library_descriptor = std::pair<cfunction_t, const char*>;
	namespace lib
	{
		static constexpr library_descriptor base =    { &luaopen_base,     "base" };
		static constexpr library_descriptor package = { &luaopen_package,  "package" };
		static constexpr library_descriptor string =  { &luaopen_string,   "string" };
		static constexpr library_descriptor table =   { &luaopen_table,    "table" };
		static constexpr library_descriptor math =    { &luaopen_math,     "math" };
		static constexpr library_descriptor io =      { &luaopen_io,       "io" };
		static constexpr library_descriptor os =      { &luaopen_os,       "os" };
		static constexpr library_descriptor debug =   { &luaopen_debug,    "debug" };
		static constexpr library_descriptor bit =     { &luaopen_bit,      "bit32" };
		static constexpr library_descriptor ffi =     { &luaopen_ffi,      "ffi" };
		static constexpr library_descriptor jit =     { &luaopen_jit,      "jit" };
	};

	// Load result.
	//
	template<Reference Ref>
	struct load_result : Ref, detail::lazy_invocable<load_result<Ref>>
	{
		int retval;

		template<typename... Tx>
		explicit load_result( int retval, Tx&&... args ) : Ref( std::forward<Tx>( args )... ), retval( retval ) {}
		
		inline bool is_error() const { return retval != 0; }
		inline bool is_success() const { return retval == 0; }
		inline void assert() const { if ( is_error() ) detail::error( Ref::state(), error() ); }
		inline explicit operator bool() const { return is_success(); }
		const char* error() const { Ref::push(); return stack::pop<const char*>( Ref::state() ); }

		std::string to_string() const 
		{
			if ( is_success() )
				return "<script>";
			else
				return error();
		}
	};

	// State.
	//
	struct state_view
	{
		lua_State* const L;

		inline state_view( lua_State* state ) : L( state ) {}
		operator lua_State*() const { return L; }

		// Sets the panic function.
		//
		inline void set_panic( cfunction_t func ) { lua_atpanic( L, func ); }

		// Creates a table.
		//
		template<typename T = table>
		inline T make_table( reserve_table rsvd = {} )
		{
			stack::create_table( L, rsvd );
			return T{ L, stack::top_t{} };
		}

		// Creates a metatable.
		//
		template<typename T = table>
		inline std::pair<T, bool> make_metatable( const char* name )
		{
			bool inserted = stack::create_metatable( L, name );
			return std::pair{ T{ L, stack::top_t{} }, inserted };
		}
		
		template<typename T = stack_table>
		inline T get_metatable( const char* name )
		{
			luaL_getmetatable( L, name );
			return T{ L, stack::top_t{} };
		}

		// References globals.
		//
		inline stack_table globals() { return stack_table{ stack_reference{ L, LUA_GLOBALSINDEX } }; }
		template<typename Key> inline auto operator[]( Key&& key ) { return detail::make_table_proxy<true>( L, LUA_GLOBALSINDEX, false, std::forward<Key>( key ) ); }

		// Opens the given libraries.
		//
		inline void open_libraries( const library_descriptor& desc )
		{
			lua_pushcfunction( L, desc.first );
			lua_pushstring( L, desc.second );
			lua_call( L, 1, 1 );
			lua_setglobal( L, desc.second );
		}
		template<typename... Tx>
		inline void open_libraries( Tx&&... descriptors )
		{
			( open_libraries( descriptors ), ... );
		}

		// Parses a script and returns the chunk as a function.
		// 
		template<Reference R = stack_reference>
		inline load_result<R> load_file( const char* path )
		{
			return load_result<R>{ luaL_loadfile( L, path ), L, stack::top_t{} };
		}
		template<Reference R = stack_reference>
		inline load_result<R> load( std::string_view script, const char* name = "" )
		{
			return load_result<R>{ luaL_loadbuffer( L, script.data(), script.size(), name ), L, stack::top_t{} };
		}

		// Runs the given script and returns any parsing errors.
		//
		inline function_result script_file( const char* path )
		{
			auto result = load( path );
			if ( !result )
			{
				stack::push( L, result );
				stack::slot top = stack::top( L );
				return function_result{ L, top, top + 1, result.retval };
			}
			return result();
		}
		inline function_result script( std::string_view script, const char* name = "" )
		{
			auto result = load( script, name );
			if ( !result )
			{
				stack::push( L, result );
				stack::slot top = stack::top( L );
				return function_result{ L, top, top + 1, result.retval };
			}
			return std::move( result )();
		}

		// Collects garbage.
		//
		inline void collect_garbage()
		{
			lua_gc( L, LUA_GCCOLLECT, 0 );
		}
	};
	struct state : state_view
	{
		inline state() : state_view( luaL_newstate() ) {}
		inline ~state() { lua_close( L ); }
	};
};
