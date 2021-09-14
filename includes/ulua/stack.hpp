#pragma once
#include "common.hpp"
#include "lua_types.hpp"

namespace ulua::stack
{
	using slot = int;
	struct top_t { inline operator slot() const { return -1; } };
	
	inline slot top( lua_State* L ) { return ( slot ) ( L->top - L->base ); }
	inline void set_top( lua_State* L, slot i ) { lua_settop( L, i ); }
	inline void pop_n( lua_State* L, slot i ) { L->top -= i; }
	inline value_type type( lua_State* L, slot i ) { return ( value_type ) lua_type( L, i ); }
	inline void copy( lua_State* L, slot i ) { lua_pushvalue( L, i ); }
	
	inline constexpr bool is_relative( slot i ) { return i < 0 && i > LUA_REGISTRYINDEX; }
	inline constexpr bool is_global( slot i ) { return i < LUA_REGISTRYINDEX; }
	inline constexpr bool is_absolute( slot i ) { return i > 0; }
	
	inline slot abs( lua_State* L, slot i ) { return is_relative( i ) ? top( L ) + 1 + i : i; }
	inline slot rel( lua_State* L, slot i ) { return is_relative( i ) ? i : i - ( top( L ) + 1 ); }
	inline void remove( lua_State* L, slot i, size_t n = 1 ) 
	{ 
		while( n )
			lua_remove( L, i + --n );
	}
	template<typename T> concept Poppable = requires { &type_traits<T>::pop; };
	template<typename T>
	inline decltype( auto ) pop( lua_State* L )
	{
		if constexpr ( Poppable<T> )
		{
			return type_traits<T>::pop( L );
		}
		else
		{
			decltype( auto ) result = type_traits<T>::get( L, top_t{} );
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
	
	// Argument pack helpers.
	//
	inline void push( lua_State* L ) {}
	template<typename T, typename... Tx>
	inline void push( lua_State* L, T&& value, Tx&&... rest )
	{
		type_traits<T>::push( L, std::forward<T>( value ) );
		push( L, std::forward<Tx>( rest )... );
	}
	template<typename... Tx> requires( sizeof...( Tx ) > 1 )
	inline auto pop( lua_State* L )
	{
		return std::forward_as_tuple( pop<Tx>( L )... );
	}
	template<typename... Tx> requires( sizeof...( Tx ) > 1 )
	inline auto get( lua_State* L, slot i )
	{
		return std::forward_as_tuple( get<Tx>( L, i++ )... );
	}
	
	// Tuple helpers.
	//
	template<typename Tup> requires detail::is_tuple_v<std::decay_t<Tup>>
	inline void push_all( lua_State* L, Tup&& args )
	{
		std::apply( [ & ] <typename... Tx> ( Tx&&... val )
		{
			push( L, std::forward<Tx>( val )... );
		}, std::forward<Tup>( args ) );
	}
	template<typename Tup> requires detail::is_tuple_v<std::decay_t<Tup>>
	inline auto pop_all( lua_State* L )
	{
		return [ & ] <template<typename...> typename R, typename... Tx> ( std::type_identity<R<Tx...>> )
		{
			return std::forward_as_tuple( pop<Tx>( L )... );
		}( std::type_identity<std::decay_t<Tup>>{} );
	}
	template<typename Tup> requires detail::is_tuple_v<std::decay_t<Tup>>
	inline auto get_all( lua_State* L, slot index )
	{
		return [ & ] <template<typename...> typename R, typename... Tx> ( std::type_identity<R<Tx...>> )
		{
			return std::forward_as_tuple( get<Tx>( L, index++ )... );
		}( std::type_identity<std::decay_t<Tup>>{} );
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

	// Call to metamethods.
	//
	inline bool call_meta( lua_State* L, slot i, const char* field )
	{
		return luaL_callmeta( L, i, field ) != 0;
	}

	// Getter for metafields.
	//
	inline bool get_meta( lua_State* L, slot i, const char* field )
	{
		return luaL_getmetafield( L, i, field ) != 0;
	}

	// String conversion.
	//
	inline std::string to_string( lua_State* L, slot i )
	{
		value_type t = type( L, i );
		switch ( t )
		{
			case value_type::number:
			case value_type::string:         return type_traits<std::string>::get( L, i );
			case value_type::boolean:        return type_traits<bool>::get( L, i ) ? "true" : "false";
			case value_type::table:
			case value_type::userdata:
			{
				if ( call_meta( L, i, "__tostring" ) || get_meta( L, i, "__name" ) )
					return pop<std::string>( L );
				break;
			}
			default: break;
		}
		return type_name( t );
	}
};