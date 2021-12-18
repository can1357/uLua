#pragma once
#include "lua_api.hpp"
#include <tuple>
#include <algorithm>
#include <utility>
#include <variant>

#ifndef __has_builtin
	#define __has_builtin(...) 0
#endif

#define ULUA_DEBUG 1 // TODO: Remove

namespace ulua
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

	// Build mode check.
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

	namespace detail
	{
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

			static constexpr auto name = [ ] ()
			{
				constexpr std::string_view view = ctti_namer<T>::__id__<T>();
				std::array<char, view.length() + 1> data = {};
				std::copy( view.begin(), view.end(), data.data() );
				return data;
			}();

			inline constexpr operator std::string_view() const { return { &name[ 0 ], &name[ name.size() - 1 ] }; }
			inline constexpr operator const char*() const { return &name[ 0 ]; }
		};

		// Checks if the type is a tuple or a pair.
		//
		template<typename T>               struct is_tuple { static constexpr bool value = false; };
		template<typename... Tx>           struct is_tuple<std::tuple<Tx...>> { static constexpr bool value = true; };
		template<typename T1, typename T2> struct is_tuple<std::pair<T1, T2>> { static constexpr bool value = true; };
		template<typename T>
		static constexpr bool is_tuple_v = is_tuple<T>::value;

		// Checks if the type is a variant.
		//
		template<typename T>               struct is_variant { static constexpr bool value = false; };
		template<typename... Tx>           struct is_variant<std::variant<Tx...>> { static constexpr bool value = true; };
		template<typename T>
		static constexpr bool is_variant_v = is_variant<T>::value;
	
		// Remove noexcept.
		//
		template<typename T> struct remove_noexcept { using type = T; };
		template<typename C, typename R, typename... Tx>     struct remove_noexcept<R(C::*)(Tx...) noexcept>             { using type = R(C::*)(Tx...); };
		template<typename C, typename R, typename... Tx>     struct remove_noexcept<R(C::*)(Tx..., ...) noexcept>        { using type = R(C::*)(Tx..., ...); };
		template<typename C, typename R, typename... Tx>     struct remove_noexcept<R(C::*)(Tx...) const noexcept>       { using type = R(C::*)(Tx...) const; };
		template<typename C, typename R, typename... Tx>     struct remove_noexcept<R(C::*)(Tx..., ...)  const noexcept> { using type = R(C::*)(Tx..., ...) const; };
		template<typename T>
		using remove_noexcept_t = typename remove_noexcept<T>::type;

		// Checks if the type is a member function or not.
		//
		template<typename T>                                 struct is_member_function { static constexpr bool value = false; };
		template<typename C, typename R, typename... Tx>     struct is_member_function<R(C::*)(Tx...)>             { static constexpr bool value = true; };
		template<typename C, typename R, typename... Tx>     struct is_member_function<R(C::*)(Tx..., ...)>        { static constexpr bool value = true; };
		template<typename C, typename R, typename... Tx>     struct is_member_function<R(C::*)(Tx...) const>       { static constexpr bool value = true; };
		template<typename C, typename R, typename... Tx>     struct is_member_function<R(C::*)(Tx..., ...)  const> { static constexpr bool value = true; };
		template<typename T>
		static constexpr bool is_member_function_v = is_member_function<remove_noexcept_t<T>>::value;
	
		// Checks if the type is a member field or not.
		//
		template<typename T>                                 struct is_member_field { static constexpr bool value = false; };
		template<typename C, typename T>                     struct is_member_field<T C::*> { static constexpr bool value = true; };
		template<typename T>
		static constexpr bool is_member_field_v = is_member_field<T>::value;

		// Callable check.
		//
		template<typename T, typename... Args>
		concept Callable = requires( T&& x ) { x( std::declval<Args>()... ); };

		// Constructible check.
		//
		template<typename T, typename... Args>
		concept Constructible = requires{ T( std::declval<Args>()... ); };

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
		template<typename Tuple, typename F> requires is_tuple_v<std::remove_const_t<Tuple>>
		static constexpr void enum_tuple( Tuple& tuple, F&& fn )
		{
			enum_indices<std::tuple_size_v<std::remove_const_t<Tuple>>>( [ & ] <size_t N> ( const_tag<N> ) { fn( std::get<N>( tuple ) ); } );
		}

		// Parameter pack helper.
		//
		template<size_t N, typename... Tx>
		struct nth_parameter;
		template<size_t N, typename T, typename... Tx>
		struct nth_parameter<N, T, Tx...> { using type = typename nth_parameter<N - 1, Tx...>::type; };
		template<typename T, typename... Tx>
		struct nth_parameter<0, T, Tx...> { using type = T; };
		template<size_t N, typename... Tx>
		using nth_parameter_t = typename nth_parameter<N, Tx...>::type;

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
		struct function_traits {};
		template<typename F> requires ( !std::is_same_v<remove_noexcept_t<F>, F> )
		struct function_traits<F> : function_traits<remove_noexcept_t<F>> {};

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
		inline bool const_eq( const char* a, const char* b )
		{
			auto interleave = [ & ] <typename T> ( std::type_identity<T> )
			{
				constexpr size_t delta = N - sizeof( T );
				T v1 = *( const T* ) b;
				v1 ^= *( const T* ) a;
				if constexpr ( delta != 0 )
				{
					using T2 = 
							std::conditional_t<delta == 1, uint8_t, 
							std::conditional_t<delta == 2, uint16_t,
							std::conditional_t<delta == 4, uint32_t,
							std::conditional_t<delta == 8, uint64_t, void>>>>;
					if constexpr ( !std::is_void_v<T2> )
					{
						T2 v2 = *( const T2* ) ( b + sizeof( T ) );
						v2 ^= *( const T2* ) ( a + sizeof( T ) );
						v1 |= v2;
					}
					else
					{
						T v2 = *( const T* ) ( b + delta );
						v2 ^= *( const T* ) ( a + delta );
						v1 |= v2;
					}
				}
				return v1 == 0;
			};
			if constexpr ( N >= 16 )     return const_eq<8>( a, b ) && const_eq<N - 8>( a + 8, b + 8 );
			else if constexpr ( N >= 8 ) return interleave( std::type_identity<uint64_t>{} );
			else if constexpr ( N >= 4 ) return interleave( std::type_identity<uint32_t>{} );
			else if constexpr ( N >= 2 ) return interleave( std::type_identity<uint16_t>{} );
			else if constexpr ( N >= 1 ) return interleave( std::type_identity<uint8_t>{} );
			else return true;
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
      	return fn( ulua::constant<size_t(i)>() ); \
      }											       \
      assume_unreachable();

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
			if ( std::is_constant_evaluated() )
			{
				auto apply = [ & ] <size_t N> ( const_tag<N>, auto&& self ) -> decltype( auto )
				{
					if constexpr ( N != Count )
					{
						if ( N == n )
							return fn( const_tag<N>{} );
						else
							return self( const_tag<N + 1>{}, self );
					}
					else
					{
						return fn( const_tag<0ull>{} );
					}
				};
				return apply( const_tag<0ull>{}, apply );
			}
			else
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
		}

		// Variant visitor.
		//
		template<typename Variant, typename F> requires is_variant_v<Variant>
		static constexpr decltype( auto ) visit_variant( Variant&& var, F&& fn )
		{
			return visit_index<std::variant_size_v<Variant>>( var.index(), [ & ] <size_t N> ( const_tag<N> ) -> decltype( auto )
			{ 
				return fn( std::move( std::get<N>( var ) ) );
			} );
		}
		template<typename Variant, typename F> requires is_variant_v<std::remove_const_t<Variant>>
		static constexpr decltype( auto ) visit_variant( Variant& var, F&& fn )
		{
			return visit_index<std::variant_size_v<std::remove_const_t<Variant>>>( var.index(), [ & ] <size_t N> ( const_tag<N> ) -> decltype( auto )
			{ 
				return fn( std::get<N>( var ) );
			} );
		}
	};

	// Errors.
	//
	static constexpr size_t max_error_length = 256;

	template<typename... Tx>
	ULUA_COLD inline void error [[noreturn]] ( lua_State* L, const char* fmt, Tx... args )
	{
		if constexpr ( sizeof...( Tx ) != 0 )
		{
			char buffer[ max_error_length ];
			snprintf( buffer, max_error_length, fmt, args... );
			buffer[ max_error_length - 1 ] = 0;
			luaL_error( L, "%s", buffer );
		}
		else
		{
			luaL_error( L, "%s", fmt );
		}
		detail::assume_unreachable();
	}
	template<typename... Tx>
	ULUA_COLD inline void arg_error [[noreturn]] ( lua_State* L, int arg, const char* fmt, Tx... args )
	{
		if constexpr ( sizeof...( Tx ) != 0 )
		{
			char buffer[ max_error_length ];
			snprintf( buffer, max_error_length, fmt, args... );
			buffer[ max_error_length - 1 ] = 0;
			luaL_argerror( L, arg, buffer );
		}
		else
		{
			luaL_argerror( L, arg, fmt );
		}
		detail::assume_unreachable();
	}
	template<typename... Tx>
	ULUA_COLD inline void type_error [[noreturn]] ( lua_State* L, int arg, const char* fmt, Tx... args )
	{
		if constexpr ( sizeof...( Tx ) != 0 )
		{
			char buffer[ max_error_length ];
			snprintf( buffer, max_error_length, fmt, args... );
			buffer[ max_error_length - 1 ] = 0;
			luaL_typerror( L, arg, buffer );
		}
		else
		{
			luaL_typerror( L, arg, fmt );
		}
		detail::assume_unreachable();
	}
};