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
	struct load_result
	{
		registry_reference ref;
		int loadfile_result;
		
		inline bool is_error() const { return loadfile_result != 0; }
		inline bool is_success() const { return loadfile_result == 0; }
		inline void assert() const { if ( is_error() ) detail::error( ref.state(), error() ); }
		inline explicit operator bool() const { return is_success(); }

		const char* error() const { ref.push(); return stack::pop<const char*>( ref.state() ); }

		template<typename... Tx>
		function_result operator()( Tx&&... args ) const
		{
			ref.push(); 
			return detail::pcall( ref.state(), std::forward<Tx>( args )... );
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
		inline table make_table( size_t num_arr = 0, size_t num_rec = 0 )
		{
			lua_createtable( L, num_arr, num_rec );
			return table{ registry_reference{ L, stack::top_t{} } };
		}

		// Creates a metatable.
		//
		template<typename T = table>
		inline std::pair<T, bool> make_metatable( const char* name )
		{
			bool inserted = luaL_newmetatable( L, name ) == 1;
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
		inline auto operator[]( const char* name ) { return detail::make_table_proxy<false>( L, LUA_GLOBALSINDEX, false, name ); }

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
		inline load_result load_file( const char* path )
		{
			int r = luaL_loadfile( L, path );
			return { registry_reference{ L, stack::top_t{} }, r };
		}
		inline load_result load( std::string_view script, const char* name = "" )
		{
			int r = luaL_loadbuffer( L, script.data(), script.size(), name );
			return { registry_reference{ L, stack::top_t{} }, r };
		}

		// Runs the given script and returns any parsing errors.
		//
		inline function_result script_file( const char* path )
		{
			auto result = load( path );
			if ( !result )
			{
				stack::push( L, result.ref );
				stack::slot top = stack::top( L );
				return function_result{ L, top, top + 1, result.loadfile_result };
			}
			return result();
		}
		inline function_result script( std::string_view script, const char* name = "" )
		{
			auto result = load( script, name );
			if ( !result )
			{
				stack::push( L, result.ref );
				stack::slot top = stack::top( L );
				return function_result{ L, top, top + 1, result.loadfile_result };
			}
			return result();
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
