#pragma once
#include <lua.hpp>
extern "C" {
	#include <lj_obj.h>
};

#include <tuple>
#include <algorithm>
#include <utility>
namespace ulua::detail
{
	// Constant tag.
	//
	template<auto V>
	struct const_tag
	{
		using type = decltype( V );
		static constexpr type value = V;
		inline constexpr operator type() { return value; };
	};
	template<auto V> inline constexpr const_tag<V> constant() { return {}; }

	// Checks if the type is a tuple or a pair.
	//
	template<typename T>               struct is_tuple { static constexpr bool value = false; };
	template<typename... Tx>           struct is_tuple<std::tuple<Tx...>> { static constexpr bool value = true; };
	template<typename T1, typename T2> struct is_tuple<std::pair<T1, T2>> { static constexpr bool value = true; };
	template<typename T>
	static constexpr bool is_tuple_v = is_tuple<T>::value;
	
	// Tuple enumeration.
	//
	template<typename Tuple, typename F, size_t N = 0>
	static constexpr void enum_tuple( Tuple&& tuple, F&& fn )
	{
		if constexpr ( N < std::tuple_size_v<std::decay_t<Tuple>> )
		{
			fn( std::get<N>( tuple ) );
			enum_tuple<Tuple, F, N + 1>( std::forward<Tuple>( tuple ), std::forward<F>( fn ) );
		}
	}

	// Function traits.
	//
	template<typename F>
	struct function_traits;

	// Function pointers:
	//
	template<typename R, typename... Tx>
	struct function_traits<R(*)(Tx...)>
	{
		static constexpr bool is_lambda = false;
		static constexpr bool is_vararg = false;

		using return_type = R;
		using arguments =   std::tuple<Tx...>;
		using owner =       void;
	};
	template<typename R, typename... Tx>
	struct function_traits<R(*)(Tx..., ...)> : function_traits<R(*)(Tx...)>
	{
		static constexpr bool is_vararg = true;
	};

	// Member functions:
	//
	template<typename C, typename R, typename... Tx>
	struct function_traits<R(C::*)(Tx...)>
	{
		static constexpr bool is_lambda = false;
		static constexpr bool is_vararg = false;

		using return_type = R;
		using arguments =   std::tuple<Tx...>;
		using owner =       C;
	};
	template<typename C, typename R, typename... Tx>
	struct function_traits<R(C::*)(Tx..., ...)> : function_traits<R(C::*)(Tx...)>
	{
		static constexpr bool is_vararg = true;
	};
	template<typename C, typename R, typename... Tx>
	struct function_traits<R(C::*)(Tx...) const>
	{
		static constexpr bool is_lambda = false;
		static constexpr bool is_vararg = false;

		using return_type = R;
		using arguments =   std::tuple<Tx...>;
		using owner =       const C;
	};
	template<typename C, typename R, typename... Tx>
	struct function_traits<R(C::*)(Tx..., ...) const> : function_traits<R(C::*)(Tx...) const>
	{
		static constexpr bool is_vararg = true;
	};

	// Lambdas or callables.
	//
	template<typename F>
	concept CallableObject = requires { &F::operator(); };
	template<CallableObject F>
	struct function_traits<F> : function_traits<decltype( &F::operator() )>
	{
		static constexpr bool is_lambda = true;
	};

	template<typename F>
	concept Invocable = requires{ typename function_traits<F>::arguments; };


	// Errors.
	//
	template<typename... Tx>
	inline void error [[noreturn]] ( lua_State* L, const char* fmt, Tx... args )
	{
		// TODO: Debug, remove.
		printf( "<< error: %s >>\n", fmt, args... );
		luaL_error( L, fmt, args... );
		unreachable();
	}

	inline void type_error [[noreturn]] ( lua_State* L, int arg, const char* type )
	{
		// TODO: Debug, remove.
		printf( "<< type error (%d): %s >>\n", arg, type );
		luaL_typerror( L, arg, type );
		unreachable();
	}
};