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
#if ULUA_JIT
		static constexpr library_descriptor ffi =     { &luaopen_ffi,      "ffi" };
		static constexpr library_descriptor jit =     { &luaopen_jit,      "jit" };
#endif
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
		inline void assert() const { if ( is_error() ) error( Ref::state(), error() ); }
		inline explicit operator bool() const { return is_success(); }
		const char* error() const { Ref::push(); return stack::pop<const char*>( Ref::state() ); }

		// Decay to function result for state helpers.
		//
		inline function_result decay_to_invocation() requires ( std::is_same_v<Ref, stack_reference> )
		{
			if constexpr ( is_debug() )
				if ( Ref::slot() != stack::top( Ref::state() ) )
					ulua::error( Ref::state(), ">> Decay from non-top slot <<" );
			Ref::release();
			stack::slot top = stack::top( Ref::state() );
			return function_result{ Ref::state(), top, top + 1, retval };
		}

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
	namespace detail
	{
		template<typename T>
		concept HasState = requires( T&& x ) { ( lua_State* ) x.state(); };
	};
	struct state_view
	{
		lua_State* L;

		// Null state view.
		//
		inline state_view() : L( nullptr ) {}
		
		// Created from lua_State directly or indirecly by any other object that has a state.
		//
		inline state_view( lua_State* state ) : L( state ) {}
		template<detail::HasState T> inline state_view( T&& ref ) : state_view( ref.state() ) {}
		
		// Decay to state and bool.
		//
		operator lua_State*() const { return L; }
		explicit operator bool() const { return L != nullptr; }

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
			auto result = load<stack_reference>( path );
			return std::move( result )( );
		}
		inline function_result script( std::string_view script, const char* name = "" )
		{
			auto result = load<stack_reference>( script, name );
			if ( !result ) return result.decay_to_invocation();
			return std::move( result )( );
		}

		template<Reference Ref>
		inline function_result script_file( const char* path, const basic_environment<Ref>& env )
		{
			auto result = load<stack_reference>( path );
			env.set_on( result );
			return std::move( result )();
		}
		template<Reference Ref>
		inline function_result script( std::string_view script, const basic_environment<Ref>& env, const char* name = "" )
		{
			auto result = load<stack_reference>( script, name );
			if ( !result ) return result.decay_to_invocation();
			env.set_on( result );
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
		using state_view::operator lua_State*;

		// Resets the lua state.
		//
		inline void reset() 
		{
			lua_close( L );
			L = luaL_newstate();
		}
	};
};
