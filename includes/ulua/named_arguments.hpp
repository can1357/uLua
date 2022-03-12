#pragma once
#include "stack.hpp"
#include "lua_types.hpp"

namespace ulua
{
	namespace detail
	{
		template<typename T, T... V>
		struct const_string
		{
			static constexpr T data[] = { V..., 0 };
			static constexpr const char* get() noexcept { return &data[ 0 ]; }
		};
	};

	// Named arguments.
	//
	template<typename T, auto N>
	struct named : T
	{ 
		template<typename... Tx>
		inline constexpr named( Tx&&... v ) : T( std::forward<Tx>( v )... ) {}
	};
	template<typename T, auto N> requires std::is_fundamental_v<T>
	struct named<T, N>
	{
		T value;
		template<typename... Tx>
		inline constexpr named( Tx&&... v ) : value( std::forward<Tx>( v )... ) {}
	
		inline constexpr operator T&()  & { return value; }
		inline constexpr operator T&&() && { return std::move( value ); }
		inline constexpr operator const T&() const & { return value; }
	};
	template<typename T, auto N>
	using named_opt = named<std::optional<T>, N>;

	// Define type traits.
	//
	template<typename T, auto N>
	struct type_traits<named<T, N>>
	{
		ULUA_INLINE inline static bool check( lua_State* L, int& idx )
		{
			stack::get_field( L, idx, ( const char* ) N.get() );
			bool result = stack::check<T>( L, stack::top_t{} );
			stack::pop_n( L, 1 );
			return result;
		}
		ULUA_INLINE inline static named<T, N> get( lua_State* L, int& idx )
		{
			stack::get_field( L, idx, ( const char* ) N.get() );
			return stack::pop<T>( L );
		}
	};
};

template<typename T, T... V>
static constexpr auto operator""_n() { return ulua::detail::const_string<T, V...>{}; }