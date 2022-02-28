#pragma once
#include <string>
#include "common.hpp"
#include "lua_types.hpp"

namespace ulua { using raw_t = std::bool_constant<true>; };

namespace ulua
{
	struct reserve_table 
	{ 
		int arr = 0; 
		int rec = 0; 

		inline constexpr reserve_table() {}
		inline constexpr explicit reserve_table( int arr, int rec ) : arr( arr ), rec( rec ) {} 
	};
	struct reserve_array   : reserve_table { inline constexpr reserve_array( int i ) :   reserve_table( i, 0 ) {} };
	struct reserve_records : reserve_table { inline constexpr reserve_records( int i ) : reserve_table( 0, i ) {} };
};

namespace ulua 
{ 
	// Registry key wrapper.
	//
	struct reg_key { int key = LUA_NOREF; };

	// Dereferencing of a registry key.
	//
	inline void unref( lua_State* L, reg_key key )
	{
		luaL_unref( L, LUA_REGISTRYINDEX, key.key );
	}
};

namespace ulua::stack
{
	using slot = int;
	struct top_t { inline operator slot() const { return -1; } };
	
	// Gets the top of the stack.
	//
	inline slot top( lua_State* L ) 
	{ 
#if ULUA_ACCEL
		slot s = accel::top( L );
#else
		slot s = lua_gettop( L );
#endif
		detail::assume_true( s >= 0 ); 
		return s; 
	}
	
	// Sets the top of the stack.
	//
	inline void set_top( lua_State* L, slot i ) { lua_settop( L, i ); }

	// Compares two slots for equality.
	//
	inline bool equals( lua_State* L, slot a, slot b ) { return lua_equal( L, a, b ) == 1; }

	// Gets the object length.
	//
	inline size_t length( lua_State* L, slot a ) { return lua_objlen( L, a ); }

	// Pushes a given value on the stack.
	//
	template<typename T>
	inline int push( lua_State* L, T&& value )
	{
		return type_traits<T>::push( L, std::forward<T>( value ) );
	}

	// Push by emplace.
	//
	template<typename T, typename... Tx>
	inline int emplace( lua_State* L, Tx&&... args )
	{
		if constexpr ( Emplacable<T> )
			return type_traits<T>::emplace( L, std::forward<T>( value ) );
		else
			return type_traits<T>::push( L, { std::forward<T>( value ) } );
	}

	// Pops a given number of items from the top of the stack.
	//
	inline void pop_n( lua_State* L, size_t i ) 
	{
#if ULUA_ACCEL
		accel::pop( L, i );
#else
		lua_pop( L, i );
#endif
	}

	// Removes a number of stack elements at the given position.
	//
	inline void remove( lua_State* L, slot i, size_t n = 1 )
	{
		while ( n )
			lua_remove( L, i + --n );
	}

	// Pushes a given item on the stack to the top of the stack.
	//
	inline void copy( lua_State* L, slot src ) { lua_pushvalue( L, src ); }
	inline void copy( lua_State* L, slot src, slot dst ) { lua_copy( L, src, dst ); }
	
	// Slot traits.
	//
	inline constexpr bool is_relative( slot i ) { return i < 0 && i > LUA_REGISTRYINDEX; }
	inline constexpr bool is_special( slot i ) { return i <= LUA_REGISTRYINDEX; }
	inline constexpr bool is_upvalue( slot i ) { return i < LUA_GLOBALSINDEX; }
	inline constexpr bool is_absolute( slot i ) { return i > 0; }
	
	// Conversion between absolute and relative slot indexing.
	//
	inline constexpr slot abs( lua_State* L, slot i ) 
	{ 
		if ( !is_relative( i ) ) return i;
		else                     return top( L ) + 1 + i; 
	}
	inline constexpr slot rel( lua_State* L, slot i ) 
	{
		if ( is_relative( i ) ) return i;
		else                    return i - ( top( L ) + 1 );
	}

	template<typename T>
	inline decltype( auto ) pop( lua_State* L )
	{
		if constexpr ( Poppable<T> )
		{
			return type_traits<T>::pop( L );
		}
		else
		{
			slot s = top_t{};
			decltype( auto ) result = type_traits<T>::get( L, s );
			pop_n( L, 1 );
			return result;
		}
	}
	template<typename T>
	inline bool check( lua_State* L, slot i )
	{
		return type_traits<T>::check( L, i );
	}
	template<typename T>
	inline decltype( auto ) get( lua_State* L, slot i )
	{
		return type_traits<T>::get( L, i );
	}

	// Pops the top of the stack into registry and returns the registry key.
	//
	inline reg_key pop_reg( lua_State* L )
	{
		return reg_key{ luaL_ref( L, LUA_REGISTRYINDEX ) };
	}

	// Pushes a value from registry into stack.
	//
	inline void push_reg( lua_State* L, reg_key key )
	{
		lua_rawgeti( L, LUA_REGISTRYINDEX, key.key );
	}
	
	// Pushes the function at the given stack level, returns false on error.
	//
	inline bool push_callstack( lua_State* L, int level )
	{
		lua_Debug debug;
		if ( !lua_getstack( L, level, &debug ) )
			return false;
		if ( !lua_getinfo( L, "f", &debug ) )
			return false;
		return true;
	}
	inline bool push_caller( lua_State* L ) { return push_callstack( L, 1 ); }

	// Moves N values from one thread to another.
	//
	inline void xmove( lua_State* from, lua_State* to, int count )
	{
		lua_xmove( from, to, count );
	}
	
	// Fetches a specific key from the table and pushes it.
	//
	template<typename T, bool Raw = false>
	inline void get_field( lua_State* L, slot i, const T& key, std::bool_constant<Raw> = {} )
	{
		if constexpr ( std::is_same_v<T, meta> )
		{
			get_field( L, i, metafield_name( key ), std::bool_constant<Raw>{} );
		}
		else if constexpr ( Raw )
		{
			if constexpr ( std::is_integral_v<T> )
			{
				lua_rawgeti( L, i, key );
			}
			else
			{
				push( L, key );
				lua_rawget( L, i );
			}
		}
		else
		{
			if constexpr ( std::convertible_to<T, const char*> )
			{
				lua_getfield( L, i, ( const char* ) key );
			}
			else if constexpr ( std::is_same_v<T, std::string> )
			{
				lua_getfield( L, i, key.data() );
			}
			else
			{
				push( L, key );
				lua_gettable( L, i );
			}
		}
	}

	// Pops a value off of the stack and assigns the value to the given field of the table.
	//
	template<typename T, bool Raw = false>
	inline void set_field( lua_State* L, slot i, const T& key, std::bool_constant<Raw> = {} )
	{
		if constexpr ( std::is_same_v<T, meta> )
		{
			set_field( L, i, metafield_name( key ), std::bool_constant<Raw>{} );
		}
		else if constexpr ( Raw )
		{
			if constexpr ( std::is_integral_v<T> )
			{
				lua_rawseti( L, i, ( int ) key );
			}
			else
			{
				push( L, key );
#if ULUA_ACCEL
				accel::xchg( L, -1, -2 );
				lua_rawset( L, i );
#else
				copy( L, -2 );
				lua_rawset( L, i );
				pop_n( L, 1 );
#endif
			}
		}
		else
		{
			if constexpr ( std::convertible_to<T, const char*> )
			{
				lua_setfield( L, i, ( const char* ) key );
			}
			else if constexpr ( std::is_same_v<T, std::string> )
			{
				lua_setfield( L, i, key.data() );
			}
			else
			{
				push( L, key );
#if ULUA_ACCEL
				accel::xchg( L, -1, -2 );
				lua_settable( L, i );
#else
				copy( L, -2 );
				lua_settable( L, i );
				pop_n( L, 1 );
#endif
			}
		}
	}

	// Creates a table and pushes it on stack.
	//
	inline static void create_table( lua_State* L, reserve_table rsvd = {} )
	{
		lua_createtable( L, rsvd.arr, rsvd.rec );
	}

	// Creates a metatable identified by a key and pushed it on stack, returns whether or not it was newly inserted.
	//
	inline static bool create_metatable( lua_State* L, const char* key )
	{
		return luaL_newmetatable( L, key ) == 1;
	}

	// Pushes the metatable for a given object.
	//
	inline static bool push_metatable( lua_State* L, slot i )
	{
		return lua_getmetatable( L, i ) != 0;
	}

	// Pops the metatable and sets it for a given object.
	//
	inline static void set_metatable( lua_State* L, slot i )
	{
		lua_setmetatable( L, i );
	}

	// Calls a metafield of the given object, if existant pushes the result on top of the stack and returns true, else does nothing.
	//
	template<typename... Tx>
	inline bool call_meta( lua_State* L, slot i, meta field, Tx&&... args )
	{
		push( L,std::forward_as_tuple( std::forward<Tx>( args )... ) );
		if ( luaL_callmeta( L, i, metafield_name( field ) ) != 0 )
			return true;
		pop_n( L, sizeof...( args ) );
		return false;
	}

	// Gets a metafield of the given object, if existant pushes it on top of the stack and returns true, else does nothing.
	//
	inline bool get_meta( lua_State* L, slot i, meta field )
	{
		return luaL_getmetafield( L, i, metafield_name( field ) ) != 0;
	}

	// Gets the type of the value in the stack slot.
	//
	inline value_type type( lua_State* L, slot i ) 
	{
		return ( value_type ) lua_type( L, i );
	}

	// Pushes a closure value popping N values into its upvalues.
	//
	inline int push_closure( lua_State* L, cfunction_t fn, slot upvalue_count = 0 )
	{
		if ( !upvalue_count )
		{
			lua_pushlightuserdata( L, fn );
			lua_rawget( L, LUA_REGISTRYINDEX );
			if ( check<cfunction_t>( L, top_t{} ) ) [[likely]]
				return 1;
			pop_n( L, 1 );

			lua_pushcclosure( L, fn, 0 );
			lua_pushlightuserdata( L, fn );
			copy( L, -2 );
			lua_rawset( L, LUA_REGISTRYINDEX );
			return 1;
		}

		lua_pushcclosure( L, fn, upvalue_count );
		return 1;
	}

	// String conversion.
	//
	ULUA_COLD static std::string to_string( lua_State* L, slot i )
	{
		value_type t = type( L, i );
		switch ( t )
		{
			case value_type::number:         return std::to_string( type_traits<double>::get( L, i ) );
			case value_type::string:         return type_traits<std::string>::get( L, i );
			case value_type::boolean:        return type_traits<bool>::get( L, i ) ? "true" : "false";
			default:
			{
				if ( call_meta( L, i, meta::tostring ) || get_meta( L, i, meta::name ) )
					return pop<std::string>( L );
				else
					return type_name( t );
			}
		}
	}

	// Pushes a type as userdata.
	//
	template<typename T, typename... Tx>
	inline T& emplace_userdata( lua_State* L, Tx&&... args )
	{
		return *new ( lua_newuserdata( L, sizeof( T ) ) ) T( std::forward<Tx>( args )... );
	}

	// Dumps the stack on console.
	//
	ULUA_COLD static void dump_stack( lua_State* L )
	{
		printf( "[[ STACK DUMP, TOP = %d ]]\n", top( L ) );
		for ( slot s = 1; s <= top( L ); s++ )
		{
			std::string res = to_string( L, s );
			if ( res.size() > 32 )
				printf( " Stack[%d] = '%.32s...'\n", s, res.data() );
			else
				printf( " Stack[%d] = '%.*s'\n", s, ( int ) res.size(), res.data() );
		}
	}
	// Same as above, except it assumes the value is at the top of the stack.
	//
	ULUA_COLD static void validate_remove( lua_State* L, slot i, size_t n = 1 )
	{
		if ( n && slot( i + n ) != slot( top( L ) + 1 ) ) [[unlikely]]
		{
			printf( ">> Remove from non-top slot detected while removing (%d, %d). <<\n", i, i + int( n ) - 1 );
			dump_stack( L );
			detail::breakpoint();
		}
	}
	inline void checked_remove( lua_State* L, [[maybe_unused]] slot i, size_t n = 1 )
	{
		if constexpr ( is_debug() )
			validate_remove( L, i, n );
		pop_n( L, n );
	}
};