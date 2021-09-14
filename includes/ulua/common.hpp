#pragma once
#include <lua.hpp>
extern "C" {
	#include <lj_obj.h>
};

#include <tuple>
#include <algorithm>
#include <utility>

#ifndef __has_builtin
	#define __has_builtin(...) 0
#endif

#define ULUA_DEBUG 1 // TODO: Remove

namespace ulua::detail
{
	// Build mode.
	//
	static constexpr bool is_debug()
	{
#if ULUA_DEBUG
		return true;
#elif NDEBUG
		return false;
#elif _DEBUG               
		return true;
#else
		return false;
#endif
	}

	// Compile time type namer.
	//
	template<typename T>
	struct ctti_namer
	{
		template<typename __id__ = T>
		static constexpr std::string_view __id__()
		{
			auto [sig, begin, delta, end] = std::tuple{
#if defined(__GNUC__) || defined(__clang__)
				std::string_view{ __PRETTY_FUNCTION__ }, std::string_view{ "__id__" }, +3, "]"
#else
				std::string_view{ __FUNCSIG__ },         std::string_view{ "__id__" }, +1, ">"
#endif
			};

			// Find the beginning of the name.
			//
			size_t f = sig.size();
			while ( sig.substr( --f, begin.size() ).compare( begin ) != 0 )
				if ( f == 0 ) return "";
			f += begin.size() + delta;

			// Find the end of the string.
			//
			auto l = sig.find_first_of( end, f );
			if ( l == std::string::npos )
				return "";

			// Return the value.
			//
			return sig.substr( f, l - f );
		}
		inline constexpr operator std::string_view() const { return ctti_namer<T>::__id__<T>(); }
	};

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
	
	// Checks if the type is a member function or not.
	//
	template<typename T>                                 struct is_member_function { static constexpr bool value = false; };
	template<typename C, typename R, typename... Tx>     struct is_member_function<R(C::*)(Tx...)>             { static constexpr bool value = true; };
	template<typename C, typename R, typename... Tx>     struct is_member_function<R(C::*)(Tx..., ...)>        { static constexpr bool value = true; };
	template<typename C, typename R, typename... Tx>     struct is_member_function<R(C::*)(Tx...) const>       { static constexpr bool value = true; };
	template<typename C, typename R, typename... Tx>     struct is_member_function<R(C::*)(Tx..., ...)  const> { static constexpr bool value = true; };
	template<typename T>
	static constexpr bool is_member_function_v = is_member_function<T>::value;

	// Constant series.
	//
	template<size_t N, typename F, size_t I = 0>
	static constexpr void enum_indices( F&& fn )
	{
		if constexpr ( I < N )
		{
			fn( const_tag<I>{} );
			return enum_indices<N, F, I + 1>( std::forward<F>( fn ) );
		}
	}

	// Tuple enumeration.
	//
	template<typename Tuple, typename F> requires is_tuple_v<Tuple>
	static constexpr void enum_tuple( Tuple&& tuple, F&& fn )
	{
		enum_indices<std::tuple_size_v<Tuple>>( [ & ] <size_t N> ( const_tag<N> ) { fn( std::move( std::get<N>( tuple ) ) ); } );
	}
	template<typename Tuple, typename F> requires is_tuple_v<Tuple>
	static constexpr void enum_tuple( Tuple& tuple, F&& fn )
	{
		enum_indices<std::tuple_size_v<Tuple>>( [ & ] <size_t N> ( const_tag<N> ) { fn( std::get<N>( tuple ) ); } );
	}
	template<typename Tuple, typename F> requires is_tuple_v<Tuple>
	static constexpr void enum_tuple( const Tuple& tuple, F&& fn )
	{
		enum_indices<std::tuple_size_v<Tuple>>( [ & ] <size_t N> ( const_tag<N> ) { fn( std::get<N>( tuple ) ); } );
	}

	// Ordering helper.
	//
	template<typename... Tx>
	struct ordered_forward_as_tuple
	{
		std::tuple<Tx...> value;
		inline constexpr ordered_forward_as_tuple( Tx&&... values ) : value( std::forward<Tx>( values )... ) {}
		inline auto unwrap() && { return std::move( value ); }
	};
	template<typename... Tx>
	ordered_forward_as_tuple( Tx&&... )->ordered_forward_as_tuple<Tx...>;
	
	template<typename T1, typename T2>
	struct ordered_forward_as_pair
	{
		std::pair<T1, T2> value;
		inline constexpr ordered_forward_as_pair( T1&& a, T2&& b ) : value( std::forward<T1>( a ), std::forward<T2>( b ) ) {}
		inline auto unwrap() && { return std::move( value ); }
	};
	template<typename T1, typename T2>
	ordered_forward_as_pair( T1&&, T2&& )->ordered_forward_as_pair<T1, T2>;

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
	//template<typename T> struct validate_member_reference;
	//template<typename C, typename Ret, typename... Tx> struct validate_member_reference<Ret( C::* )( Tx... )> { validate_member_reference( ... ) {} };
	//template<typename C, typename Ret, typename... Tx> struct validate_member_reference<Ret( C::* )( Tx..., ... )>{ validate_member_reference( ... ) {} };
	//template<typename C, typename Ret, typename... Tx> struct validate_member_reference<Ret( C::* )( Tx... ) const>{ validate_member_reference( ... ) {} };
	//template<typename C, typename Ret, typename... Tx> struct validate_member_reference<Ret( C::* )( Tx..., ... ) const>{ validate_member_reference( ... ) {} };
	//template<typename T> validate_member_reference( T )->validate_member_reference<T>;

	template<typename F>
	concept CallableObject = requires{ is_member_function_v<decltype(&F::operator())>; };

	template<CallableObject F>
	struct function_traits<F> : function_traits<decltype( &F::operator() )>
	{
		static constexpr bool is_lambda = true;
	};

	template<typename F>
	concept Invocable = requires{ typename function_traits<F>::arguments; };

	// Compiler specifics.
	//
#if defined(__GNUC__) || defined(__clang__)
	#define ULUA_COLD [[gnu::noinline, gnu::cold]]
#elif _MSC_VER
	#define ULUA_COLD __declspec(noinine)
#endif
	
	inline void breakpoint()
	{
#if __has_builtin(__builtin_debugtrap)
		__builtin_debugtrap();
#elif defined(_MSC_VER)
		__debugbreak();
#endif
	}
	inline constexpr void assume_true( bool condition )
	{
#if __has_builtin(__builtin_assume)
		__builtin_assume( condition );
#elif defined(_MSC_VER)
		__assume( condition );
#endif
	}
	inline void assume_unreachable [[noreturn]] ()
	{
#if __has_builtin(__builtin_unreachable)
		__builtin_unreachable();
#else
		assume_true( false );
#endif
	}
	template<size_t N>
	inline bool const_eq( const void* a, const void* b )
	{
#if __has_builtin(__builtin_bcmp)
		return !__builtin_bcmp( a, b, N );
#elif __has_builtin(__builtin_memcmp)
		return !__builtin_memcmp( a, b, N );
#else
		using T = std::array<uint8_t, N>;
		return *( const T* ) a == *( const T* ) b;
#endif
	}

	// Errors.
	//
	template<typename... Tx>
	ULUA_COLD inline void error [[noreturn]] ( lua_State* L, const char* fmt, Tx... args )
	{
		// TODO: Debug, remove.
		printf( " [[ uLua error: '" );
		printf( fmt, args... );
		printf( "' ]]\n" );

		luaL_error( L, fmt, args... );
		assume_unreachable();
	}

	ULUA_COLD inline void type_error [[noreturn]] ( lua_State* L, int arg, const char* type )
	{
		// TODO: Debug, remove.
		printf( " [[ uLua type error: 'Slot #%d' is not '%s' ]] \n", arg, type );
		luaL_typerror( L, arg, type );
		assume_unreachable();
	}

	// Visit strategies:
	//
	namespace impl
	{
#define __visit_8(i, x)		\
		case (i+0): x(i+0);	\
		case (i+1): x(i+1);	\
		case (i+2): x(i+2);	\
		case (i+3): x(i+3);	\
		case (i+4): x(i+4);	\
		case (i+5): x(i+5);	\
		case (i+6): x(i+6);	\
		case (i+7): x(i+7);	
#define __visit_64(i, x)        \
		__visit_8(i+(8*0), x)     \
		__visit_8(i+(8*1), x)     \
		__visit_8(i+(8*2), x)     \
		__visit_8(i+(8*3), x)     \
		__visit_8(i+(8*4), x)     \
		__visit_8(i+(8*5), x)     \
		__visit_8(i+(8*6), x)     \
		__visit_8(i+(8*7), x)
#define __visit_512(i, x)        \
		__visit_64(i+(64*0), x)    \
		__visit_64(i+(64*1), x)    \
		__visit_64(i+(64*2), x)    \
		__visit_64(i+(64*3), x)    \
		__visit_64(i+(64*4), x)    \
		__visit_64(i+(64*5), x)    \
		__visit_64(i+(64*6), x)    \
		__visit_64(i+(64*7), x)
#define __visitor(i) 						       \
		if constexpr ( ( i ) < Lim ) {	       \
			return fn( const_tag<size_t(i)>{} ); \
		}											       \
		unreachable();							       \

		template<size_t Lim, typename F> inline static constexpr decltype( auto ) numeric_visit_8( size_t n, F&& fn ) { switch ( n ) { __visit_8( 0, __visitor ); default: assume_unreachable(); } }
		template<size_t Lim, typename F> inline static constexpr decltype( auto ) numeric_visit_64( size_t n, F&& fn ) { switch ( n ) { __visit_64( 0, __visitor ); default: assume_unreachable(); } }
		template<size_t Lim, typename F> inline static constexpr decltype( auto ) numeric_visit_512( size_t n, F&& fn ) { switch ( n ) { __visit_512( 0, __visitor ); default: assume_unreachable(); } }
#undef __visitor
#undef __visit_512
#undef __visit_64
#undef __visit_8
	};

	// Strict numeric visit.
	//
	template<size_t Count, typename F>
	inline static constexpr decltype( auto ) visit_index( size_t n, F&& fn )
	{
		if constexpr ( Count <= 8 )
			return impl::numeric_visit_8<Count>( n, std::forward<F>( fn ) );
		else if constexpr ( Count <= 64 )
			return impl::numeric_visit_64<Count>( n, std::forward<F>( fn ) );
		else if constexpr ( Count <= 512 )
			return impl::numeric_visit_512<Count>( n, std::forward<F>( fn ) );
		else
		{
			// Binary search.
			//
			constexpr size_t Midline = Count / 2;
			if ( n >= Midline )
				return visit_index<Count - Midline>( n - Midline, [ & ] <size_t N> ( const_tag<N> ) { fn( const_tag<N + Midline>{} ); } );
			else
				return visit_index<Midline>( n, std::forward<F>( fn ) );
		}
	}
};